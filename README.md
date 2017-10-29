# gnuboy -- for the Onion Omega2

### Description
This is a port of the original [gnuboy](https://github.com/rofl0r/gnuboy) Gameboy and Gameboy Color emulator for the Onion Omega2(+).  
It uses my [ILI9225 library](https://github.com/gamer-cndg/omega2-ili9225) to drive the display and my [Wii Classic](https://github.com/gamer-cndg/omega2-wii-classic-controller) library for the joystick input. Additionally, it uses ALSA to play audio over a USB audio card.

### Issues
Due to the problems with the SPI interface on the Omega2(+), this program inherits these problems, too. The ILI9225 SPI display can only be driven at about 2 FPS for the time being, which is due to a 16 byte TX buffer in the SPI driver. Every 8th pixel's color might be corrupted (MSB of red value) because the SPI interface has a bug, causing it to transmit the first bit of my 16-byte transfers incorrectly. See [here](https://community.onion.io/topic/2448/libili9225-controlling-a-ili9225-spi-display-with-your-omega2) if you're interested.  
I use multithreading to overcome the slowness of the display driver. The video thread just runs detached from everything else at its own pace. Audio and everything else still operates at full speed, stutter-free.  
### Hardware Wireup
You will need:
* an Onion Omega2 or Omega2+, preferably with the extension dock
* an ILI9225 display (e.g., [on ebay](https://www.ebay.com/itm/2-2-inch-LCD-2-2-SPI-TFT-LCD-Display-Module-ILI9225-with-SD-Socket-for-Arduino/162145341921))
* a Wii classic controller 
* an adapter board for the controller's connector (e.g., [Adafruit's nunchucky](https://www.adafruit.com/product/345))
* a USB audio card (can be obtained for $3 and less on ebay)
* either headphones or an amplifier chip with loudspeakers (e.g. PAM8403) to hear the sound
* breadboard and wires

The pins for SPI display are statically compiled into the binary. They are as follows: MOSI = 8, SCLK = 7, CS = 6, RST = 3, RS = 1. The Wii Classic gamepad has to be connected to the standard I2C bus, as labeled on the extension dock.


### Program Usage

Since this is a port of the gnuboy program, it can be used exactly as the original program. Usually, you just want to play a ROM using the syntax
```sh
./gnuboy <path-to-romfile>
```

The Wii classic gamepad button mappings are as follows: 

| Wii Classic | Gameboy    |
|:-----------:|:----------:|
| A           | A          |
| B           | B          |
| Start       | Start      |
| Select      | Select     |
| DPAD        | DPAD       |
| left analog stick| DPAD |
| ZL          | save state |
| ZR          | load state |

### Compilation

This project was designed for cross-compliation. Compile your toolchain according to https://docs.onion.io/omega2-docs/cross-compiling.html, **change the paths** in the `Makefile` (`TOOLCHAIN_ROOT_DIR`) and do a `make all`. Optionally, `make upload` will attempt to use `sshpass` with `scp` to transfer the compiled binary to your Omega Onion2 system. Simply change the IP address and the password if you whish to use this feature.

For a successful compilation, you need the `omega_includes` and `omega_libs` folder somewhere on your computer. You can download them [here](https://github.com/gamer-cndg/omega2-libs). Change the path in the `Makefile` accordingly. 

### Testing the compiled binaries

1. Install the dependencies library by typing `opkg update && opkg install libonionspi libonioni2c alsa-utils alsa-lib`. If you already have these libraries, skip this step.
2. Transfer the `libili9225.so` and `libwiiclassic.so` file from the `omega2-libs` repository to the `/usr/lib/` folder on your Omega2, e.g. by using `ssh` or `scp`. 
3. Make sure the wiring is that which is described in the wireup section 
4. Transfer the `gnuboy` file (ELF) to some directory on your Omega2, e.g. `/root/` and run it with the path to some ROM (`.gb` or `.gbc` file)!

### Credits
* Maximilian Gerhardt, program porter
* Laguna (from [gnuboy](https://github.com/rofl0r/gnuboy))
* All the people and companies on which dependencies are based (e.g. for the Wii controller or the ILI9225 stuff)
 
Remember that gnuboy is GPL licensed! If you modify the source and distribute the binaries, you have to release the source code.

### Media
![hardware setup](https://github.com/gamer-cndg/omega2-gnuboy/raw/master/omega_gnuboy_hw_setup2.jpg)
![games](https://github.com/gamer-cndg/omega2-gnuboy/raw/master/omega_gnuboy_compilation.png)

Video (links to youtube):
[![Watch the video](https://img.youtube.com/vi/LoHRnyid1ZQ/0.jpg)](https://www.youtube.com/watch?v=LoHRnyid1ZQ)