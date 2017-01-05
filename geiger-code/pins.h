
#ifndef PINS_H_
#define PINS_H_

#include <inttypes.h>

enum port_e
{
	PORT_B = 1,
	PORT_C,
	PORT_D,
};

typedef uint8_t pin_t;

//pin_t pin_create(enum port_e port, uint8_t pin);
#define pin_create(port, pin) ((port == PORT_B) ? (0x20 | pin) : (port == PORT_C) ? (0x40 | pin) : (0x80 | pin))
void pin_set(pin_t pin, uint8_t value);
uint8_t pin_read(pin_t pin);
void pin_set_inout(pin_t pin, uint8_t out);


#endif /* PINS_H_ */