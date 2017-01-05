

#include "pins.h"
#include <avr/io.h>


//pin_t pin_create(enum port_e port, uint8_t pin)
//{
	//return (port == PORT_B) ? (0x20 | pin) : (port == PORT_C) ? (0x40 | pin) : (0x80 | pin);
//}

void pin_set(pin_t pin, uint8_t value)
{
	volatile uint8_t *port = (pin & 0x20) ? &PORTB : (pin & 0x40) ? &PORTC : &PORTD;
	uint8_t mask = 1 << (pin & 0x07);
	*port = value ? *port | mask : *port & (~mask);
}

uint8_t pin_read(pin_t pin)
{
	volatile uint8_t *port = (pin & 0x20) ? &PINB : (pin & 0x40) ? &PINC : &PIND;
	uint8_t mask = 1 << (pin & 0x07);
	return *port & mask;
}

void pin_set_inout(pin_t pin, uint8_t out)
{
	volatile uint8_t *port = (pin & 0x20) ? &DDRB : (pin & 0x40) ? &DDRC : &DDRD;
	uint8_t mask = 1 << (pin & 0x07);
	*port = out ? *port | mask : *port & (~mask);
}
