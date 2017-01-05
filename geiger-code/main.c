
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"


struct Lcd lcd_data;
struct Lcd *lcd = &lcd_data;
uint32_t g_pulses = 0;
uint32_t g_millis = 0;

#define PIN_PIEZO pin_create(PORT_C, 0)
#define PIN_NMOSG pin_create(PORT_B, 1)

uint8_t g_piezo_beep = 0;
uint32_t g_timer1_cycles = 0;


// duty between 0 and UINT16_MAX
void set_mosfet_pwm(uint16_t duty)
{
	uint32_t duty32 = duty;
	OCR1A = (uint16_t)(duty32 * ICR1 / UINT16_MAX);
}


void init()
{
	cli(); //stop interrupts
	
	pin_set_inout(PIN_PIEZO, 1);
	pin_set_inout(PIN_NMOSG, 1);
	
	lcd_init(lcd, 1, pin_create(PORT_D, 0), pin_create(PORT_D, 1), pin_create(PORT_D, 2), 
		pin_create(PORT_D, 4), pin_create(PORT_D, 5), pin_create(PORT_D, 6), pin_create(PORT_D, 7), 0, 0, 0, 0);
	lcd_begin(lcd, 16, 2, LCD_5x8DOTS);
	
	// Pin change interrupt for handling geiger pulses
	PCICR |= (1 << PCIE1); // enable PCINT1 Interrupt Vector
	PCMSK1 |= (1 << PCINT12); // enable pin change interrupt on PCINT12 = PC4
	
	// Timer 0 used for timekeeping
	TCCR0B |= 0x5; // Timer 0 prescaler to 1024
	TIMSK0 |= (1 << TOIE0); // Enable Timer 0 interrupt on overflow
	#define CYCLES_PER_TIMER0 (1024L * 256L)
	
	// Set timer 1 for 10 bit non inverting fast pwm at 20 kHz for mosfet control
	ICR1 = F_CPU / 20000L; // 16 MHz / 20kHz = 800
	TCCR1A = (1 << COM1A1) | (1 << WGM11);
	TCCR1B = (1 << WGM12)  | (1 << WGM13) | (1 << CS10);
	OCR1A = 0; // Output 0
	
	sei(); //enable interrupts
}


// Geiger detect connected to PCINT12 -> triggers PCINT1_vect
ISR(PCINT1_vect)
{
	if ((PINC & (1 << PINC4)) == 0) // trigger on low flank
	{
		++g_pulses;
		g_piezo_beep = 1;
	}
}


//Timer0 interrupt function (called every 1024 * 256 cycles)
#define CYLCES_PER_MS (F_CPU / 1000L)
ISR(TIMER0_OVF_vect)
{
	// Add cycles to g_timer1_cycles and subtract whole milliseconds from it
	g_timer1_cycles += CYCLES_PER_TIMER0;
	uint32_t millis = g_timer1_cycles / CYLCES_PER_MS;
	g_timer1_cycles -= millis * CYLCES_PER_MS;
	g_millis += millis;
}


void loop()
{
	char string[32] = "Count: ";
	ltoa(g_pulses, string + 7, 10);
	
	lcd_home(lcd);
	lcd_write_str(lcd, string);
	
	if (g_piezo_beep)
	{
		pin_set(PIN_PIEZO, 1);
		_delay_us(100);
		pin_set(PIN_PIEZO, 0);
	}
	_delay_ms(1);
}


int main(void)
{
	init();
	
    while (1) 
    {
		loop();
    }
}

