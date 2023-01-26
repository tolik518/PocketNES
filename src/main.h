#ifndef __MAIN_H__
#define __MAIN_H__

#define NORETURN __attribute__ ((noreturn))

#ifdef __cplusplus
extern "C" {
#endif

extern u32 oldinput;
//extern const unsigned int __fp_status_arm;
extern u8 *textstart;//points to first NES rom (initialized by boot.s)
extern u8 *ewram_start;
extern u8 *end_of_exram;
extern int roms;//total number of roms
extern char pogoshell_romname[32];	//keep track of rom name (for state saving, etc)
extern u32 pogoshell_filesize;
extern char pogoshell;
extern char rtc;
extern char gameboyplayer;
extern char gbaversion;
extern const int ne;

#define PALTIMING 4

void C_entry(void);
void splash(const u16* splashimage);
void jump_to_rommenu(void) NORETURN;
void rommenu(void);
u8 *findrom(int n);
int drawmenu(int sel);
int getinput(void);
void cls(int chrmap);
//void drawtext(int row,char *str,int hilite);
void setdarknessgs(int dark);
void setbrightnessall(int light);

#if FLASHCART
	#define AGB_ROM  ((u8*)0x8000000)
	#define AGB_SRAM ((u8*)0xE000000)
	#define AGB_SRAM_SIZE 64*1024
	#define _FLASH_WRITE(pa, pd) { *(((u16 *)AGB_ROM)+((pa)/2)) = pd; __asm("nop"); }
	extern u32 total_rom_size;
	extern u32 flash_sram_area;
	extern u8 flash_type;
	extern u32 get_flash_type();
	void flash_write(u8 flash_type, u32 sa);
	void save_sram_FLASH();
#endif

#ifdef __cplusplus
}
#endif

#endif
