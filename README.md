# PocketNES
Forked from [Dwedit's](https://github.com/Dwedit/PocketNES) repository.

# Build ROM
## With pnesbuild
With [pnesbuild](/nespack/pnesbuild.py) you can build the rom via command line with

```sh
./pnesbuild.py -o Collection.gba ../path/to/pocketnes.gba Thwaite.nes 'Nova the Squirrel.nes'
```

or you could give it a whole folder with NES files.
```sh
./pnesbuild.py -o Collection.gba ../path/to/pocketnes.gba romsfolder/*.nes`
```


# Modifications
## Batteryless saving
This allows PocketNES to save its SRAM data on most modern reproduction/bootleg flash cartridges that have an SRAM chip but no battery installed. This works by writing the SRAM data into Flash ROM memory and restoring the data from Flash ROM into SRAM when booting.

The Flash ROM is updated in the following instances:
- Pressing L+R to bring up the main menu (with a prompt)
- Writing a Save State or Quick Save State by pressing R+SELECT
- Deleting a Save State or SRAM
- Exiting

The batteryless PocketNES SRAM storage area in Flash ROM is dynamically determined and depends on the maximum ROM size of the flash cartridge. In case you need to extract/inject/edit something, you will find the data at ROM address `flash_size - 0x40000` and the length is 0x10000 bytes. Emulators should also be able to extract the SRAM data into a `.sav` file from a re-dumped compilation.

If you need to manually specify the save data location in ROM, you can append the compiled ROM with `53 56 4C 43` (SVLC) followed by the address in little endian (`00 00 0C 00` would be offset `0xC0000` on the flash chip). Make sure the address is correctly aligned to your flash chip�s sector size and that it�s accessible from the GBA ROM space (max. 32 MB).

Holding SELECT+UP+B on startup will wipe the save data in case something is not working right.

Tested repro cartridge flash chips: 128W30B, 256L30B, 29LV128DT, M36L0R7050T, M36L0R8060T, GL128S, M29W128FH, MSP55LV128M

## Other modifications
* added some more illegal OP codes by [NovaSquirrel](https://github.com/NovaSquirrel/PocketNES) (adding more compatibility)

* added pnesbuild.py by [pinobatch](https://github.com/pinobatch) (for building ROM in CLI)
