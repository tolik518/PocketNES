#include "includes.h"

/*
#include <stdio.h>
#include <string.h>
#include "gba.h"
#include "minilzo.107/minilzo.h"
#include "cheat.h"
#include "asmcalls.h"
#include "main.h"
#include "ui.h"
#include "sram.h"
*/

extern romheader mb_header;

//const unsigned __fp_status_arm=0x40070000;
EWRAM_BSS u8 *textstart;//points to first NES rom (initialized by boot.s)
EWRAM_BSS u8 *ewram_start;
EWRAM_BSS u8 *end_of_exram;
EWRAM_BSS u32 max_multiboot_size;
EWRAM_BSS u32 oldinput;

EWRAM_BSS char pogoshell_romname[32];	//keep track of rom name (for state saving, etc)
#if RTCSUPPORT
EWRAM_BSS char rtc=0;
#endif
EWRAM_BSS char pogoshell=0;
EWRAM_BSS char gameboyplayer=0;
EWRAM_BSS char gbaversion;
const int ne=0x454e;

#if SAVE
EWRAM_BSS extern u8* BUFFER1;
EWRAM_BSS extern u8* BUFFER2;
EWRAM_BSS extern u8* BUFFER3;
#endif

#if FLASHCART
u32 total_rom_size = 0;
u32 flash_size = 0;
u32 flash_sram_area = 0;
u8 flash_type = 0;
#endif

#if GCC
EWRAM_BSS u32 copiedfromrom=0;

int strstr_(const char *str1, const char *str2);

__attribute__((section(".append")))
int main()
{
	//set text_start (before moving the rom)
	extern u8 __rom_end__[];
	extern u8 __eheap_start[];
	u32 end_addr = (u32)(&__rom_end__);
	textstart = (u8*)(end_addr);
	u32 heap_addr = (u32)(&__eheap_start);
	ewram_start = (u8*)heap_addr;
	
	extern u8 MULTIBOOT_LIMIT[];
	u32 multiboot_limit = (u32)(&MULTIBOOT_LIMIT);
	max_multiboot_size = multiboot_limit - heap_addr;
	
	if (end_addr < 0x08000000 && copiedfromrom)
	{
		textstart += (0x08000000 - 0x02000000);
	}

	C_entry();
	return 0;
}

#endif


__attribute__((section(".append")))
void C_entry()
{
	int i;
	u32 temp;
	
#if RTCSUPPORT
	vu16 *timeregs=(u16*)0x080000c8;
	*timeregs=1;
	if(*timeregs & 1) rtc=1;
#endif
	
	scaling = 3;  //default mode is scaled with spirtes
	
#if !GCC
	ewram_start = (u8*)	&Image$$RO$$Limit;
	if (ewram_start>=(u8*)0x08000000)
	{
		ewram_start=(u8*)0x02000000;
	}
#endif
	end_of_exram = (u8*)&END_OF_EXRAM;
	
	okay_to_run_hdma = 0;
	irqInit();
	//key,vblank,timer1,timer2,timer3,serial interrupt enable
	irqSet(IRQ_TIMER1,timer1interrupt);
	irqSet(IRQ_TIMER2,timer_2_interrupt);
	irqSet(IRQ_TIMER3,timer_3_interrupt);
	irqSet(IRQ_SERIAL,serialinterrupt);
	irqSet(IRQ_VCOUNT,vcountinterrupt);
	irqSet(IRQ_HBLANK,hblankinterrupt);
	irqEnable(IRQ_KEYPAD | IRQ_VBLANK | IRQ_TIMER1 | IRQ_TIMER2 | IRQ_TIMER3 | IRQ_SERIAL);
	REG_DISPSTAT &= ~(LCDC_HBL | LCDC_VCNT);
	REG_IE |= IRQ_VCOUNT | IRQ_HBLANK;
	//Warning: VRAM code must be loaded before the Vblank handler is installed
	
	//Do the fade before anything else so we can fade to black from Pogoshell's screen.  Doesn't work in newest pogoshell anymore.
	if(REG_DISPCNT==FORCE_BLANK)	//is screen OFF?
		REG_DISPCNT=0;				//screen ON
	*MEM_PALETTE=0x7FFF;			//white background
	REG_BLDCNT=0x00ff;				//brightness decrease all
	for(i=0;i<17;i++) {
		REG_BLDY=i;					//fade to black
		waitframe();
	}
	*MEM_PALETTE=0;					//black background (avoids blue flash when doing multiboot)
	REG_DISPCNT=0;					//screen ON, MODE0
	
	memset32((u32*)0x6000000,0,0x18000);  //clear vram (fixes graphics junk)
	//Warning: VRAM code must be loaded at some point
	
	temp=(u32)(*(u8**)0x0203FBFC);
	pogoshell=((temp & 0xFE000000) == 0x08000000)?1:0;
	gbaversion=CheckGBAVersion();
	
	//load font+palette
	loadfont();
	loadfontpal();
	ui_x=0x100;
	move_ui();
//	REG_BG2HOFS=0x0100;		//Screen left
	REG_BG2CNT=0x0400;	//16color 512x256 CHRbase0 SCRbase6 Priority0
	
	extern void init_speed_hacks();
	init_speed_hacks();
	
	//PPU_init();
	build_chr_decode();
	#if CRASH
	crash_disabled = 1;
	#endif
	
//	PPU_reset();
	
#if SAVE
	BUFFER1 = ewram_start;
	BUFFER2 = BUFFER1+0x10000;
	BUFFER3 = BUFFER2+0x20000;
#endif

	if(pogoshell)
	{
		//find the filename
		char *s=(char*)0x0203fc08;
		//0203FC08 contains argv[0], 00, then argv[1].
		//advance past first null
		while (*s++ != 0);
		//advance past second null
		while (*s++ != 0);
		//return to slash or null
		s-=2;
		while (*s != '/' && *s != 0)
		{
			s--;
		}
		s++;
		//copy rom name
		bytecopy(pogoshell_romname,s,32);
		
		//check for PAL-like filename
		if(strstr_(s,"(E)") || strstr_(s,"(e)"))		//Check if it's a European rom.
			emuflags |= PALTIMING;
		else
			emuflags &= ~PALTIMING;
		
		//set ROM address
		textstart=(*(u8**)0x0203FBFC)-sizeof(romheader);
		
		//So it will try to detect roms when loading state
		roms=1;
		
#if MULTIBOOT
		memcpy(mb_header.name,pogoshell_romname,32);
#endif
	}

	if (!pogoshell)
	{
		bool wantToSplash = false;
		const u16 *splashImage = NULL;
		u8 *p;
		u32 nes_id=0x1a530000+ne;
		if (find_nes_header(textstart+sizeof(romheader))==NULL)
		{
			wantToSplash = true;
			splashImage = (const u16*)textstart;
			textstart+=76800;
		}

		i=0;
		p=find_nes_header(textstart);
		while(p && *(u32*)(p+48)==nes_id)
		{
			//count roms
			p+=*(u32*)(p+32)+48;
#if FLASHCART
			total_rom_size=p-0x8000000;
#endif
			p=find_nes_header(p);
			i++;
		}
		roms=i;
		
#if FLASHCART
		flash_type = get_flash_type();
		flash_sram_area = 0;
		
		// Override SRAM flash locationif ROM is appended with "SVLC" followed by the address to use
		if (*(u32 *)(AGB_ROM+total_rom_size) == 0x434C5653) {
			flash_sram_area = *(u32 *)(AGB_ROM+total_rom_size+4);
		}
		
		if (flash_type > 0) {
			// Determine the size of the flash chip by checking for ROM loops,
			// then set the SRAM storage area 0x40000 bytes before the end.
			// This is due to different sector sizes of different flash chips,
			// and should hopefully cover all cases.
			if (memcmp(AGB_ROM+4, AGB_ROM+4+0x400000, 0x40) == 0) {
				flash_size = 0x400000;
			} else if (memcmp(AGB_ROM+4, AGB_ROM+4+0x800000, 0x40) == 0) {
				flash_size = 0x800000;
			} else if (memcmp(AGB_ROM+4, AGB_ROM+4+0x1000000, 0x40) == 0) {
				flash_size = 0x1000000;
			} else {
				flash_size = 0x2000000;
			}
			if (flash_sram_area == 0) {
				flash_sram_area = flash_size - 0x40000;
			}
			
			// RIP if the selected storage area is within the PocketNES ROM...
			if (total_rom_size > flash_sram_area) {
				get_ready_to_display_text();
				cls(3);
				ui_x=0;
				move_ui();
				drawtext(7," The PocketNES compilation",0);
				drawtext(8,"  is too large for saving",0);
				drawtext(9,"  SRAM data in Flash ROM.",0);
				drawtext(11,"Please remove some ROMs from",0);
				drawtext(12,"      the compilation.",0);
				strmerge(str,"PocketNES ", VERSION_NUMBER);
				drawtext(19,str,0);
				while (1) waitframe();
			}
			
			// Finally, restore the SRAM data and proceed.
			bytecopy(AGB_SRAM, ((u8*)AGB_ROM+flash_sram_area), AGB_SRAM_SIZE);
		
		} else { // Emulator mode?
			if (flash_sram_area == 0) {
				if ((*(u32*)(AGB_ROM+0x400000-0x40000) == STATEID) || (*(u32*)(AGB_ROM+0x400000-0x40000) == STATEID2)) {
					flash_sram_area = 0x400000-0x40000;
				} else if ((*(u32*)(AGB_ROM+0x800000-0x40000) == STATEID) || (*(u32*)(AGB_ROM+0x800000-0x40000) == STATEID2)) {
					flash_sram_area = 0x800000-0x40000;
				} else if ((*(u32*)(AGB_ROM+0x1000000-0x40000) == STATEID) || (*(u32*)(AGB_ROM+0x1000000-0x40000) == STATEID2)) {
					flash_sram_area = 0x1000000-0x40000;
				} else if ((*(u32*)(AGB_ROM+0x2000000-0x40000) == STATEID) || (*(u32*)(AGB_ROM+0x2000000-0x40000) == STATEID2)) {
					flash_sram_area = 0x2000000-0x40000;
				}
			}
			if (flash_sram_area != 0) {
				bytecopy(AGB_SRAM, ((u8*)AGB_ROM+flash_sram_area), AGB_SRAM_SIZE);
				bytecopy(AGB_SRAM, ((u8*)AGB_ROM+flash_sram_area), AGB_SRAM_SIZE/2); // some emulators don't like 64 KB of SRAM, so at least give them the first 32 KB again
			}
		}
		// Failsafe: Holding SELECT+UP+B on boot will invalidate SRAM
		if (((*(u16 *)(0x4000130))) == 0x03B9) {
			*(u32 *)AGB_SRAM = 0xFFFFFFFF;
		}
#endif
		
		if (i == 0)
		{
			#if !COMPY
			get_ready_to_display_text();
			cls(3);
			ui_x=0;
			move_ui();
			drawtext( 1,"     No NES ROMs found.",0);
			drawtext( 3,"You can build a compilation",0);
			drawtext( 4,"using PocketNES Menu Maker.",0);
#if FLASHCART
			drawtext( 9," This version of PocketNES",0);
			drawtext(10,"     supports saving on",0);
			drawtext(11,"batteryless repro flashcarts.",0);
			drawtext(13,"    https://git.io/JtsMs",0);
#endif
			strmerge(str,"PocketNES ", VERSION_NUMBER);
			drawtext(19,str,0);
			#endif
			while (1)
			{
				waitframe();
			}
		}
		if (wantToSplash)
		{
			splash(splashImage);
		}
		
		if(!i)i=1;					//Stop PocketNES from crashing if there are no ROMs
		roms=i;
	}
//	REG_WININ=0xFFFF;
//	REG_WINOUT=0xFFFB;
//	REG_WIN0H=0xFF;
//	REG_WIN0V=0xFF;
	
	//load VRAM CODE
	extern u8 __vram1_start[], __vram1_lma[], __vram1_end[];
	int vram1_size = ((((u8*)__vram1_end - (u8*)__vram1_start) - 1) | 3) + 1;
	memcpy32((u32*)__vram1_start,(const u32*)__vram1_lma,vram1_size);
	
	//If multiboot, move ROM from textstart to heapstart
	//u32 end_addr = (u32)(&__rom_end__);
	if (copiedfromrom == 0 && (u32)textstart < 0x08000000 && textstart > ewram_start)
	{
		memmove32(ewram_start, textstart, 0x02040000 - (u32)textstart);
		textstart = ewram_start;
	}
	
	spriteinit();
	stop_dma_interrupts();
	
	irqSet(IRQ_VBLANK,vblankinterrupt);
	
	#if COMPY
		build_byte_reverse_table();
	#endif
	
	#if SAVE
		lzo_init();	//init compression lib for savestates
	#endif
	
//	LZ77UnCompVram(&font,(u16*)0x6002400);
//	memcpy((void*)0x5000080,&fontpal,64);
	
	
	#if SAVE
		readconfig();
	#endif
	jump_to_rommenu();
}

void jump_to_rommenu()
{
#if GCC
	extern u8 __sp_usr[];
	u32 newstack=(u32)(&__sp_usr);
	__asm__ volatile ("mov sp,%0": :"r"(newstack));
#else
	__asm {mov r0,#0x3007f00}		//stack reset
	__asm {mov sp,r0}
#endif
	rommenu();
	run(1);
	while (true);
}

//show splash screen
__attribute__((section(".append")))
void splash(const u16 *image) {
	int i;

	REG_DISPCNT=FORCE_BLANK;	//screen OFF
	memcpy32((u32*)MEM_VRAM,(const u32*)image,240*160*2);
	waitframe();
//	REG_BG2CNT=0x0000;
	REG_DISPCNT=BG2_EN|MODE3;
	for(i=16;i>=0;i--) {	//fade from white
		setbrightnessall(i);
		waitframe();
	}
	for(i=0;i<150;i++) {	//wait 2.5 seconds
		waitframe();
		if (REG_P1==0x030f){
			gameboyplayer=1;
			gbaversion=3;
		}
	}
	memset32((u32*)0x6000000,0,0x18000);  //clear vram (fixes graphics junk)
	get_ready_to_display_text();
	loadfont();
	ui_x=0;
	move_ui();
}

#if COMPY
__attribute__((section(".append")))
void build_byte_reverse_table()
{
	extern const u8 byte_reverse_table_init[256];
	extern u8 byte_reverse_table[256];

	//u8 *byte_reverse_table = 0x02000100;
	memcpy32(byte_reverse_table, byte_reverse_table_init, 0x100);
}

#endif

//0x02000000


int strlen_(const char *str)
{
	int len = 0;
	for (;;)
	{
		if (*str == 0) return len;
		str++;
		len++;
	}
}

#if FLASHCART
// This function will auto-detect three common
// types of reproduction flash cartridges.
// Must run in EWRAM because ROM data is
// not visible to the system while checking.
__attribute__((section(".ewram")))
u32 get_flash_type() {
	u32 rom_data, data;
	u16 ie = REG_IE;
	stop_dma_interrupts();
	REG_IE = ie & 0xFFFE;
	
	rom_data = *(u32 *)AGB_ROM;
	
	// Type 1 or 4
	_FLASH_WRITE(0, 0xFF);
	_FLASH_WRITE(0, 0x90);
	data = *(u32 *)AGB_ROM;
	_FLASH_WRITE(0, 0xFF);
	if (rom_data != data) {
		// Check if the chip is responding to this command
		// which then needs a different write command later
		_FLASH_WRITE(0x59, 0x42);
		data = *(u8 *)(AGB_ROM+0xB2);
		_FLASH_WRITE(0x59, 0x96);
		_FLASH_WRITE(0, 0xFF);
		if (data != 0x96) {
			REG_IE = ie;
			resume_interrupts();
			return 4;
		}
		REG_IE = ie;
		resume_interrupts();
		return 1;
	}
	
	// Type 2
	_FLASH_WRITE(0, 0xF0);
	_FLASH_WRITE(0xAAA, 0xA9);
	_FLASH_WRITE(0x555, 0x56);
	_FLASH_WRITE(0xAAA, 0x90);
	data = *(u32 *)AGB_ROM;
	_FLASH_WRITE(0, 0xF0);
	if (rom_data != data) {
		REG_IE = ie;
		resume_interrupts();
		return 2;
	}
	
	// Type 3
	_FLASH_WRITE(0, 0xF0);
	_FLASH_WRITE(0xAAA, 0xAA);
	_FLASH_WRITE(0x555, 0x55);
	_FLASH_WRITE(0xAAA, 0x90);
	data = *(u32 *)AGB_ROM;
	_FLASH_WRITE(0, 0xF0);
	if (rom_data != data) {
		REG_IE = ie;
		resume_interrupts();
		return 3;
	}
	
	REG_IE = ie;
	resume_interrupts();
	return 0;
}

// This function will issue a flash sector erase
// operation at the given sector address and then
// write 64 kilobytes of SRAM data to Flash ROM.
// Must run in EWRAM because ROM data is
// not visible to the system while erasing/writing.
__attribute__((section(".ewram")))
void flash_write(u8 flash_type, u32 sa)
{
	if (flash_type == 0) return;
	u16 ie = REG_IE;
	stop_dma_interrupts();
	REG_IE = ie & 0xFFFE;
	
	if (flash_type == 1) {
		// Erase flash sector
		_FLASH_WRITE(sa, 0xFF);
		_FLASH_WRITE(sa, 0x60);
		_FLASH_WRITE(sa, 0xD0);
		_FLASH_WRITE(sa, 0x20);
		_FLASH_WRITE(sa, 0xD0);
		while (1) {
			__asm("nop");
			if (*(((u16 *)AGB_ROM)+(sa/2)) == 0x80) {
				break;
			}
		}
		_FLASH_WRITE(sa, 0xFF);
		
		// Write data
		for (int i=0; i<AGB_SRAM_SIZE; i+=2) {
			_FLASH_WRITE(sa+i, 0x40);
			_FLASH_WRITE(sa+i, (*(u8 *)(AGB_SRAM+i+1)) << 8 | (*(u8 *)(AGB_SRAM+i)));
			while (1) {
				__asm("nop");
				if (*(((u16 *)AGB_ROM)+(sa/2)) == 0x80) {
					break;
				}
			}
		}
		_FLASH_WRITE(sa, 0xFF);
	
	} else if (flash_type == 2) {
		// Erase flash sector
		_FLASH_WRITE(sa, 0xF0);
		_FLASH_WRITE(0xAAA, 0xA9);
		_FLASH_WRITE(0x555, 0x56);
		_FLASH_WRITE(0xAAA, 0x80);
		_FLASH_WRITE(0xAAA, 0xA9);
		_FLASH_WRITE(0x555, 0x56);
		_FLASH_WRITE(sa, 0x30);
		while (1) {
			__asm("nop");
			if (*(((u16 *)AGB_ROM)+(sa/2)) == 0xFFFF) {
				break;
			}
		}
		_FLASH_WRITE(sa, 0xF0);
		
		// Write data
		for (int i=0; i<AGB_SRAM_SIZE; i+=2) {
			_FLASH_WRITE(0xAAA, 0xA9);
			_FLASH_WRITE(0x555, 0x56);
			_FLASH_WRITE(0xAAA, 0xA0);
			_FLASH_WRITE(sa+i, (*(u8 *)(AGB_SRAM+i+1)) << 8 | (*(u8 *)(AGB_SRAM+i)));
			while (1) {
				__asm("nop");
				if (*(((u16 *)AGB_ROM)+((sa+i)/2)) == ((*(u8 *)(AGB_SRAM+i+1)) << 8 | (*(u8 *)(AGB_SRAM+i)))) {
					break;
				}
			}
		}
		_FLASH_WRITE(sa, 0xF0);
	
	} else if (flash_type == 3) {
		// Erase flash sector
		_FLASH_WRITE(sa, 0xF0);
		_FLASH_WRITE(0xAAA, 0xAA);
		_FLASH_WRITE(0x555, 0x55);
		_FLASH_WRITE(0xAAA, 0x80);
		_FLASH_WRITE(0xAAA, 0xAA);
		_FLASH_WRITE(0x555, 0x55);
		_FLASH_WRITE(sa, 0x30);
		while (1) {
			__asm("nop");
			if (*(((u16 *)AGB_ROM)+(sa/2)) == 0xFFFF) {
				break;
			}
		}
		_FLASH_WRITE(sa, 0xF0);
		
		// Write data
		for (int i=0; i<AGB_SRAM_SIZE; i+=2) {
			_FLASH_WRITE(0xAAA, 0xAA);
			_FLASH_WRITE(0x555, 0x55);
			_FLASH_WRITE(0xAAA, 0xA0);
			_FLASH_WRITE(sa+i, (*(u8 *)(AGB_SRAM+i+1)) << 8 | (*(u8 *)(AGB_SRAM+i)));
			while (1) {
				__asm("nop");
				if (*(((u16 *)AGB_ROM)+((sa+i)/2)) == ((*(u8 *)(AGB_SRAM+i+1)) << 8 | (*(u8 *)(AGB_SRAM+i)))) {
					break;
				}
			}
		}
		_FLASH_WRITE(sa, 0xF0);

	} else if (flash_type == 4) {
		// Erase flash sector
		_FLASH_WRITE(sa, 0xFF);
		_FLASH_WRITE(sa, 0x60);
		_FLASH_WRITE(sa, 0xD0);
		_FLASH_WRITE(sa, 0x20);
		_FLASH_WRITE(sa, 0xD0);
		while (1) {
			__asm("nop");
			if ((*(((u16 *)AGB_ROM)+(sa/2)) & 0x80) == 0x80) {
				break;
			}
		}
		_FLASH_WRITE(sa, 0xFF);
		
		// Write data
		int c = 0;
		while (c < AGB_SRAM_SIZE) {
			_FLASH_WRITE(sa+c, 0xEA);
			while (1) {
				__asm("nop");
				if ((*(((u16 *)AGB_ROM)+((sa+c)/2)) & 0x80) == 0x80) {
					break;
				}
			}
			_FLASH_WRITE(sa+c, 0x1FF);
			for (int i=0; i<1024; i+=2) {
				_FLASH_WRITE(sa+c+i, (*(u8 *)(AGB_SRAM+c+i+1)) << 8 | (*(u8 *)(AGB_SRAM+c+i)));
			}
			_FLASH_WRITE(sa+c, 0xD0);
			while (1) {
				__asm("nop");
				if ((*(((u16 *)AGB_ROM)+((sa+c)/2)) & 0x80) == 0x80) {
					break;
				}
			}
			_FLASH_WRITE(sa+c, 0xFF);
			c += 1024;
		}
	}
	
	REG_IE = ie;
	resume_interrupts();
}

void save_sram_FLASH()
{
	if (flash_type == 0) return;
	flash_write(flash_type, flash_sram_area);
}
#endif

//newlib's version is way too big (over 1k vs 32 bytes!)
int strstr_(const char *str1, const char *str2)
{
	int len1 = strlen_(str1);
	int len2 = strlen_(str2);
	int l = len1 - len2;
	for (int i=0; i < l; i++)
	{
		int j;
		for (j = 0; j < len2; j++)
		{
			if (str1[i + j] != str2[j])
			{
				break;
			}
		}
		if (j == len2)
		{
			return i;
		}
	}
	return 0;
}
