
#include "lcd.h"
#include <util/delay.h>

#define LOW 0
#define HIGH 1
#define OUTPUT 1


void lcd_init(struct Lcd *lcd, uint8_t fourbitmode, pin_t rs, pin_t rw, pin_t enable,
	pin_t d0, pin_t d1, pin_t d2, pin_t d3,
	pin_t d4, pin_t d5, pin_t d6, pin_t d7)
{
	lcd->_rs_pin = rs;
	lcd->_rw_pin = rw;
	lcd->_enable_pin = enable;
	
	lcd->_data_pins[0] = d0;
	lcd->_data_pins[1] = d1;
	lcd->_data_pins[2] = d2;
	lcd->_data_pins[3] = d3;
	lcd->_data_pins[4] = d4;
	lcd->_data_pins[5] = d5;
	lcd->_data_pins[6] = d6;
	lcd->_data_pins[7] = d7;

	if (fourbitmode)
		lcd->_displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	else
		lcd->_displayfunction = LCD_8BITMODE | LCD_1LINE | LCD_5x8DOTS;
}


void lcd_begin(struct Lcd *lcd, uint8_t cols, uint8_t lines, uint8_t dotsize) 
{
	if (lines > 1)
		lcd->_displayfunction |= LCD_2LINE;
	lcd->_numlines = lines;

	lcd_setRowOffsets(lcd, 0x00, 0x40, 0x00 + cols, 0x40 + cols);

	// for some 1 line displays you can select a 10 pixel high font
	if ((dotsize != LCD_5x8DOTS) && (lines == 1))
		lcd->_displayfunction |= LCD_5x10DOTS;

	pin_set_inout(lcd->_rs_pin, OUTPUT);
	// we can save 1 pin by not using RW. Indicate by passing 255 instead of pin#
	if (lcd->_rw_pin != 255)
		pin_set_inout(lcd->_rw_pin, OUTPUT);
	pin_set_inout(lcd->_enable_pin, OUTPUT);
	
	// Do these once, instead of every time a character is drawn for speed reasons.
	for (int i=0; i<((lcd->_displayfunction & LCD_8BITMODE) ? 8 : 4); ++i)
	{
		pin_set_inout(lcd->_data_pins[i], OUTPUT);
	}

	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way before 4.5V so we'll wait 50
	_delay_us(50000);
	// Now we pull both RS and R/W low to begin commands
	pin_set(lcd->_rs_pin, LOW);
	pin_set(lcd->_enable_pin, LOW);
	if (lcd->_rw_pin != 255)
		pin_set(lcd->_rw_pin, LOW);
	
	//put the LCD into 4 bit or 8 bit mode
	if (! (lcd->_displayfunction & LCD_8BITMODE)) 
	{
		// this is according to the hitachi HD44780 datasheet
		// figure 24, pg 46

		// we start in 8bit mode, try to set 4 bit mode
		lcd_write4bits(lcd, 0x03);
		_delay_us(4500); // wait min 4.1ms

		// second try
		lcd_write4bits(lcd, 0x03);
		_delay_us(4500); // wait min 4.1ms
		
		// third go!
		lcd_write4bits(lcd, 0x03);
		_delay_us(150);

		// finally, set to 4-bit interface
		lcd_write4bits(lcd, 0x02);
	} 
	else 
	{
		// this is according to the hitachi HD44780 datasheet
		// page 45 figure 23

		// Send function set command sequence
		lcd_command(lcd, LCD_FUNCTIONSET | lcd->_displayfunction);
		_delay_us(4500);  // wait more than 4.1ms

		// second try
		lcd_command(lcd, LCD_FUNCTIONSET | lcd->_displayfunction);
		_delay_us(150);

		// third go
		lcd_command(lcd, LCD_FUNCTIONSET | lcd->_displayfunction);
	}

	// finally, set # lines, font size, etc.
	lcd_command(lcd, LCD_FUNCTIONSET | lcd->_displayfunction);

	// turn the display on with no cursor or blinking default
	lcd->_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	lcd_display(lcd);

	// clear it off
	lcd_clear(lcd);

	// Initialize to default text direction (for romance languages)
	lcd->_displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// set the entry mode
	lcd_command(lcd, LCD_ENTRYMODESET | lcd->_displaymode);

}


void lcd_setRowOffsets(struct Lcd *lcd, int row0, int row1, int row2, int row3)
{
	lcd->_row_offsets[0] = row0;
	lcd->_row_offsets[1] = row1;
	lcd->_row_offsets[2] = row2;
	lcd->_row_offsets[3] = row3;
}

/********** high level commands, for the user! */
void lcd_clear(struct Lcd *lcd)
{
	lcd_command(lcd, LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
	_delay_us(2000);  // this command takes a long time!
}


void lcd_home(struct Lcd *lcd)
{
	lcd_command(lcd, LCD_RETURNHOME);  // set cursor position to zero
	_delay_us(2000);  // this command takes a long time!
}


void lcd_setCursor(struct Lcd *lcd, uint8_t col, uint8_t row)
{
	const size_t max_lines = sizeof(lcd->_row_offsets) / sizeof(*lcd->_row_offsets);
	if ( row >= max_lines )
		row = max_lines - 1;    // we count rows starting w/0
	if ( row >= lcd->_numlines )
		row = lcd->_numlines - 1;    // we count rows starting w/0
	
	lcd_command(lcd, LCD_SETDDRAMADDR | (col + lcd->_row_offsets[row]));
}


// Turn the display on/off (quickly)
void lcd_noDisplay(struct Lcd *lcd) 
{
	lcd->_displaycontrol &= ~LCD_DISPLAYON;
	lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->_displaycontrol);
}


void lcd_display(struct Lcd *lcd) 
{
	lcd->_displaycontrol |= LCD_DISPLAYON;
	lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->_displaycontrol);
}


// Turns the underline cursor on/off
void lcd_noCursor(struct Lcd *lcd) 
{
	lcd->_displaycontrol &= ~LCD_CURSORON;
	lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->_displaycontrol);
}


void lcd_cursor(struct Lcd *lcd) 
{
	lcd->_displaycontrol |= LCD_CURSORON;
	lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->_displaycontrol);
}


// Turn on and off the blinking cursor
void lcd_noBlink(struct Lcd *lcd) 
{
	lcd->_displaycontrol &= ~LCD_BLINKON;
	lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->_displaycontrol);
}


void lcd_blink(struct Lcd *lcd) {
	lcd->_displaycontrol |= LCD_BLINKON;
	lcd_command(lcd, LCD_DISPLAYCONTROL | lcd->_displaycontrol);
}


// These commands scroll the display without changing the RAM
void lcd_scrollDisplayLeft(struct Lcd *lcd) 
{
	lcd_command(lcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}


void lcd_scrollDisplayRight(struct Lcd *lcd) 
{
	lcd_command(lcd, LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}


// This is for text that flows Left to Right
void lcd_leftToRight(struct Lcd *lcd) 
{
	lcd->_displaymode |= LCD_ENTRYLEFT;
	lcd_command(lcd, LCD_ENTRYMODESET | lcd->_displaymode);
}


// This is for text that flows Right to Left
void lcd_rightToLeft(struct Lcd *lcd) 
{
	lcd->_displaymode &= ~LCD_ENTRYLEFT;
	lcd_command(lcd, LCD_ENTRYMODESET | lcd->_displaymode);
}


// This will 'right justify' text from the cursor
void lcd_autoscroll(struct Lcd *lcd) 
{
	lcd->_displaymode |= LCD_ENTRYSHIFTINCREMENT;
	lcd_command(lcd, LCD_ENTRYMODESET | lcd->_displaymode);
}


// This will 'left justify' text from the cursor
void lcd_noAutoscroll(struct Lcd *lcd) 
{
	lcd->_displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
	lcd_command(lcd, LCD_ENTRYMODESET | lcd->_displaymode);
}


// Allows us to fill the first 8 CGRAM locations
// with custom characters
void lcd_createChar(struct Lcd *lcd, uint8_t location, uint8_t charmap[]) 
{
	location &= 0x7; // we only have 8 locations 0-7
	lcd_command(lcd, LCD_SETCGRAMADDR | (location << 3));
	for (int i=0; i<8; i++)
		lcd_write(lcd, charmap[i]);
}

/*********** mid level commands, for sending data/cmds */

inline void lcd_command(struct Lcd *lcd, uint8_t value) 
{
	lcd_send(lcd, value, LOW);
}


inline void lcd_write(struct Lcd *lcd, uint8_t value) 
{
	lcd_send(lcd, value, HIGH);
}

void lcd_write_str(struct Lcd *lcd, const char *str)
{
	while(*str)
		lcd_write(lcd, *str++);
}

/************ low level data pushing commands **********/

// write either command or data, with automatic 4/8-bit selection
void lcd_send(struct Lcd *lcd, uint8_t value, uint8_t mode) 
{
	pin_set(lcd->_rs_pin, mode);

	// if there is a RW pin indicated, set it low to Write
	if (lcd->_rw_pin != 255) 
	{
		pin_set(lcd->_rw_pin, LOW);
	}
	
	if (lcd->_displayfunction & LCD_8BITMODE) 
	{
		lcd_write8bits(lcd, value);
	} 
	else 
	{
		lcd_write4bits(lcd, value>>4);
		lcd_write4bits(lcd, value);
	}
}


void lcd_pulseEnable(struct Lcd *lcd) 
{
	pin_set(lcd->_enable_pin, LOW);
	_delay_us(1);
	pin_set(lcd->_enable_pin, HIGH);
	_delay_us(1);    // enable pulse must be >450ns
	pin_set(lcd->_enable_pin, LOW);
	_delay_us(100);   // commands need > 37us to settle
}


void lcd_write4bits(struct Lcd *lcd, uint8_t value)
{
	for (int i = 0; i < 4; i++)
		pin_set(lcd->_data_pins[i], (value >> i) & 0x01);
	lcd_pulseEnable(lcd);
}


void lcd_write8bits(struct Lcd *lcd, uint8_t value) 
{
	for (int i = 0; i < 8; i++)
		pin_set(lcd->_data_pins[i], (value >> i) & 0x01);
	lcd_pulseEnable(lcd);
}
