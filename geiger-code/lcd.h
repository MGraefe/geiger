
#ifndef LCD_H_
#define LCD_H_

#include <stddef.h>
#include "pins.h"

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00


struct Lcd
{
	pin_t _rs_pin; // LOW: command.  HIGH: character.
	pin_t _rw_pin; // LOW: write to LCD.  HIGH: read from LCD.
	pin_t _enable_pin; // activated by a HIGH pulse.
	pin_t _data_pins[8];

	uint8_t _displayfunction;
	uint8_t _displaycontrol;
	uint8_t _displaymode;

	uint8_t _initialized;

	uint8_t _numlines;
	uint8_t _row_offsets[4];
};

void lcd_init(struct Lcd *lcd, uint8_t fourbitmode, pin_t rs, pin_t rw, pin_t enable,
	pin_t d0, pin_t d1, pin_t d2, pin_t d3,
	pin_t d4, pin_t d5, pin_t d6, pin_t d7);
	
void lcd_begin(struct Lcd *lcd, uint8_t cols, uint8_t rows, uint8_t charsize);

void lcd_clear(struct Lcd *lcd);
void lcd_home(struct Lcd *lcd);

void lcd_noDisplay(struct Lcd *lcd);
void lcd_display(struct Lcd *lcd);
void lcd_noBlink(struct Lcd *lcd);
void lcd_blink(struct Lcd *lcd);
void lcd_noCursor(struct Lcd *lcd);
void lcd_cursor(struct Lcd *lcd);
void lcd_scrollDisplayLeft(struct Lcd *lcd);
void lcd_scrollDisplayRight(struct Lcd *lcd);
void lcd_leftToRight(struct Lcd *lcd);
void lcd_rightToLeft(struct Lcd *lcd);
void lcd_autoscroll(struct Lcd *lcd);
void lcd_noAutoscroll(struct Lcd *lcd);

void lcd_setRowOffsets(struct Lcd *lcd, int row1, int row2, int row3, int row4);
void lcd_createChar(struct Lcd *lcd, uint8_t, uint8_t[]);
void lcd_setCursor(struct Lcd *lcd, uint8_t, uint8_t);
void lcd_write(struct Lcd *lcd, uint8_t);
void lcd_write_str(struct Lcd *lcd, const char *str);
void lcd_command(struct Lcd *lcd, uint8_t);

void lcd_send(struct Lcd *lcd, uint8_t, uint8_t);
void lcd_write4bits(struct Lcd *lcd, uint8_t);
void lcd_write8bits(struct Lcd *lcd, uint8_t);
void lcd_pulseEnable(struct Lcd *lcd);


#endif /* LCD_H_ */