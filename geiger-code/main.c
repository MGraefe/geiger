
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <stdlib.h>
#include <string.h>

#include "lcd.h"


struct Lcd lcd_data;
struct Lcd *lcd = &lcd_data;
uint32_t g_pulses = 0;
uint16_t g_pulses_current_second = 0;
uint32_t g_millis = 0;
uint32_t g_cpm = 0;

#define PIN_PIEZO pin_create(PORT_C, 0)
#define PIN_NMOSG pin_create(PORT_B, 1)
#define PIN_DETECT pin_create(PORT_C, 4)
#define PIN_VSENSE pin_create(PORT_C, 5)
#define NUM_IMPULSE_BINS 60

uint8_t g_piezo_beep = 0;
uint32_t g_timer1_cycles = 0;
uint32_t g_next_voltage_check = 0;
uint32_t g_next_lcd_update = 0;
uint32_t g_next_cpm_update = 0;
int16_t g_tube_voltage;
uint16_t g_tube_duty = 0;
uint32_t g_impulse_bins[NUM_IMPULSE_BINS];
uint8_t g_bins_pos = 0;
uint8_t g_bins_count = 0;

uint32_t impulse_bin_sum(uint8_t max_num)
{
	if (g_bins_count == 0)
	return 0;
	uint32_t val = 0;
	uint8_t i = (g_bins_count < NUM_IMPULSE_BINS || g_bins_pos == NUM_IMPULSE_BINS) ? 0 : g_bins_pos;
	uint8_t count = 0;
	while(count < g_bins_count && count < max_num)
	{
		val += g_impulse_bins[i++];
		if (i == NUM_IMPULSE_BINS)
			i = 0;
		count++;
	}
	return val;
}

void impulse_bin_add(uint32_t impulses)
{
	if (g_bins_pos == NUM_IMPULSE_BINS)
		g_bins_pos = 0;
	g_impulse_bins[g_bins_pos++] = impulses;
	if (g_bins_count < NUM_IMPULSE_BINS)
		g_bins_count++;
}

// duty between 0 and UINT16_MAX
void set_mosfet_pwm(uint16_t duty)
{
	uint32_t duty32 = duty;
	OCR1A = (uint16_t)(duty32 * ICR1 / UINT16_MAX);
}


uint16_t read_analog(uint8_t i)
{
	uint8_t l, h;

	ADMUX = (1 << REFS0) | (i & 0x7);
	ADCSRA |= (1 << ADSC);
	while(ADCSRA & (1 << ADSC)) {};
	l = ADCL;
	h = ADCH;
	return (h << 8) | l;
}


void init()
{
	cli(); //stop interrupts
	
	pin_set_inout(PIN_PIEZO, 1);
	pin_set_inout(PIN_NMOSG, 1);
	pin_set_inout(PIN_DETECT, 0);
	pin_set_inout(PIN_VSENSE, 0);
	pin_set(PIN_DETECT, 1); // Enable internal pullup
	
	lcd_init(lcd, 1, pin_create(PORT_D, 0), pin_create(PORT_D, 1), pin_create(PORT_D, 2), 
		pin_create(PORT_D, 4), pin_create(PORT_D, 5), pin_create(PORT_D, 6), pin_create(PORT_D, 7), 0, 0, 0, 0);
	lcd_begin(lcd, 16, 2, LCD_5x8DOTS);
	
	// Pin change interrupt for handling geiger pulses
	PCICR |= (1 << PCIE1); // enable PCINT1 Interrupt Vector
	PCMSK1 |= (1 << PCINT12); // enable pin change interrupt on PCINT12 = PC4
	
	// Timer 2 used for timekeeping
	//ASSR |= (1 < AS2); // Use external clock
	TCCR2B |= 0x7; // Timer2 prescaler to 1024
	TIMSK2 |= (1 << TOIE2); // Enable Timer 2 interrupt on overflow
	#define CYCLES_PER_TIMER2 (1024L * 256L)
	
	// Set timer 1 for 10 bit non inverting fast pwm at 20 kHz for mosfet control
	ICR1 = F_CPU / 10000L; // 8 MHz / 20kHz = 800
	TCCR1A = (1 << COM1A1) | (1 << WGM11);
	TCCR1B = (1 << WGM12)  | (1 << WGM13) | (1 << CS10);
	//TIMSK1 = (1 << TOIE1);
	OCR1A = 0; // Output 0

	// Set ADC Converter enabled and 128 prescaler
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

	// Power reduction
	PRR = (1 << PRTIM0) // Timer 0
		| (1 << PRTWI) // TWI (Two wire interface)
		| (1 << PRUSART0) // USART
		| (1 << PRSPI); // SPI

	sei(); //enable interrupts
}


// Geiger detect connected to PCINT12 -> triggers PCINT1_vect
ISR(PCINT1_vect)
{
	if ((PINC & (1 << PINC4)) == 0) // trigger on low flank
	{
		++g_pulses;
		++g_pulses_current_second;
		g_piezo_beep = 1;
	}
}


//Timer2 interrupt function (called every 1024 * 256 cycles = 32,768ms @ 8MHz)
#define CYLCES_PER_MS (F_CPU / 1000L)
ISR(TIMER2_OVF_vect)
{
	uint32_t millis;

	// Add cycles to g_timer1_cycles and subtract whole milliseconds from it
	g_timer1_cycles += CYCLES_PER_TIMER2;
	millis = g_timer1_cycles / CYLCES_PER_MS;
	g_timer1_cycles -= millis * CYLCES_PER_MS;
	g_millis += millis;
}


uint16_t clamp(uint16_t val, uint16_t min, uint16_t max)
{
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}


void voltageReg()
{
	uint16_t value;
	PRR &= ~(1 << PRADC); //enable adc in prr
	ADCSRA |= (1 << ADEN); //enable adc
	value = read_analog(5);
	ADCSRA &= ~(1 << ADEN); //disable adc
	PRR |= (1 << PRADC); //disable adc in prr

	// vcc is 5 volts and conversion factor is 1V sensed = 200V tube voltage, scale times 64 to get meaningful result
	g_tube_voltage = value * 64 / (uint16_t)((1.0f/1023.0f) * 5.0f * 200.0f * 64.0f + 0.5f);
	int16_t diff = 400 - g_tube_voltage;

	if (g_tube_voltage < 395 && g_tube_duty < (UINT16_MAX / 100 * 50))
		g_tube_duty += (UINT16_MAX / 1000) * clamp(diff / 8, 1, 20);
	else if (g_tube_voltage > 405 && g_tube_duty > (UINT16_MAX / 100 * 2))
		g_tube_duty -= (UINT16_MAX / 1000) * clamp(-diff / 8, 1, 20);
	set_mosfet_pwm(g_tube_duty);
}


/* A utility function to reverse a string  */
void reverse(char *str, int length)
{
	int start = 0;
	int end = length -1;
	char temp;
	while (start < end)
	{
		temp = str[start];
		str[start] = str[end];
		str[end] = temp;
		
		start++;
		end--;
	}
}

// Implementation of itoa()
int itoa_fill(int num, char* str, int base)
{
	int i = 0;
	uint8_t isNegative = 0;
	
	/* Handle 0 explicitely, otherwise empty string is printed for 0 */
	if (num == 0)
	{
		str[i++] = '0';
		return i;
	}
	
	// In standard itoa(), negative numbers are handled only with
	// base 10. Otherwise numbers are considered unsigned.
	if (num < 0 && base == 10)
	{
		isNegative = 1;
		num = -num;
	}
	
	// Process individual digits
	while (num != 0)
	{
		int rem = num % base;
		str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
		num = num / base;
	}
	
	// If number is negative, append '-'
	if (isNegative)
		str[i++] = '-';
		
	// Reverse the string
	reverse(str, i);
	
	return i;
}


void update_lcd()
{
	char line[32];
	memset(line, ' ', 16);

	itoa_fill(g_pulses, line, 10);

	int len = itoa_fill(g_tube_voltage, line + 3, 10);
	line[3 + len] = 'V';

	len = itoa_fill(g_tube_duty / (UINT16_MAX / 100), line + 7, 10);
	line[7 + len] = '%';

	len = itoa_fill(g_cpm, line + 11, 10);
	memcpy(line + 11 + len, "cpm", 3);

	line[16] = 0;
	lcd_home(lcd);
	lcd_write_str(lcd, line);

	memset(line, ' ', 16);
	itoa_fill(g_millis / 1000, line, 10);

	uint32_t usv = g_cpm * 10000LL / 15835LL;
	uint8_t is_msv = 0;
	if (usv > 99999)
	{
		usv /= 1000;
		is_msv = 1;
	}
	uint16_t wholeuSv = usv / 100;
	uint16_t commauSv = usv % 100;
	itoa_fill(wholeuSv, line + 5 + (wholeuSv < 10 ? 2 : wholeuSv < 100 ? 1 : 0), 10);
	line[8] = '.';
	if (commauSv < 10)
		line[9] = '0';
	itoa_fill(commauSv, line + 9 + (commauSv < 10 ? 1 : 0), 10);

	memcpy(line + 11, "uSv/h", 5);
	if (is_msv)
		line[11] = 'm';

	line[16] = 0;
	lcd_setCursor(lcd, 0, 1);
	lcd_write_str(lcd, line);
}

void loop()
{
	if (g_millis > g_next_cpm_update)
	{
		g_next_cpm_update += 1000;
		impulse_bin_add(g_pulses_current_second);
		g_pulses_current_second = 0;
		g_cpm = impulse_bin_sum(255) * (60 / (g_bins_count == 0 ? 1 : g_bins_count));
	}
	
	if (g_millis > g_next_lcd_update)
	{
		g_next_lcd_update = g_millis + 200;
		update_lcd();
	}

	if (g_piezo_beep)
	{
		pin_set(PIN_PIEZO, 1);
		_delay_us(100);
		pin_set(PIN_PIEZO, 0);
		g_piezo_beep = 0;
	}

	if (g_millis > g_next_voltage_check)
	{
		voltageReg();
		g_next_voltage_check = g_millis + 200;
	}

	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_mode();
	//_delay_ms(1);
}


int main(void)
{
	_delay_ms(200);
	init();
	
    while (1) 
    {
		loop();
    }
}

