/*
 * Battery_Charger.c
 * Created: 29/11/2016
 * Author:  PerpendicularTriangle
 * Version: 0.4

	Some battery tests, stoped charging when it reached 1.7v
	After Charge	After Hours of settling time
	1.64			1.50				Panasonic - old
	1.62			1.45				Panasonic - old
	1.48			1.32				Duracell - oldish, abused (not fully charged, took too long)
	1.60			1.46				Duracell - oldish
	1.54			1.44				Kodak - old (not fully charged, took too long)
	1.52			1.43				Kodak - old (not fully charged, took too long)

 */

// Set to 1 if using a 16Mhz External Crystal, set to 0 if using the 1Mhz Internal Oscillator
#if (1)
	#define F_CPU 16000000UL
	#define serial_port_divider 0x33	// 19K2 Baud @ 16Mhz
	#define compair_match_register 156	// ~50hz
#else
	#define F_CPU 1000000UL
	#define serial_port_divider 0x19	// 2K4 Baud @ 1Mhz
	#define compair_match_register 10	// ~50hz
#endif


// #define __DELAY_BACKWARD_COMPATIBLE__	// Set this for older compilers, like 'Atmel Studio 5'


#include <avr/io.h>			// for PORTx
#include <avr/interrupt.h>	// for sei()
#include <util/delay.h>		// for _delay_ms()
#include <avr/sleep.h>		// for sleep_mode()


#define led5_bar (unsigned char) 0x20	// Green
#define led4_bar (unsigned char) 0x10	// Green
#define led3_bar (unsigned char) 0x08	// Red
#define led2_bar (unsigned char) 0x04	// Red
#define led1_bar (unsigned char) 0x02	// Red


// Approx formula: ADC = 0.518 * mVolts + 8.663 (referenced to the resistors I used)
#define volt_2000 0x3ff
#define volt_1900 0x3e0
#define volt_1800 0x3ae
#define volt_1750 0x392
#define volt_1700 0x37E	// Stop charging value
#define volt_1650 0x360
#define volt_1600 0x346	// new Top Green Led
#define volt_1550 0x32c
#define volt_1500 0x312	// new 2nd Green Led
#define volt_1450 0x2f8
#define volt_1400 0x2DF	// new Red Led
#define volt_1350 0x2c4
#define volt_1325 0x2b7
#define volt_1300 0x2aa	// new Red Led
#define volt_1200 0x277
#define volt_1150 0x25d
#define volt_1100 0x242
#define volt_1000 0x20F
#define volt_0975 0x201
#define volt_0900 0x1db
#define volt_0800 0x1a8	// Bottom Red Led
#define volt_0700 0x174
#define volt_0000 0x020	// Zero volts for no battery, Highest level including noise


unsigned char battery_charging = 0;	// 4 bits for which batteries are charging (sending the pulses)
unsigned char which_pulse = 0;		// boolean for the 50% duty cycle

unsigned char checking_battery = 0;	// current battery being checked every few seconds
unsigned char fully_charged = 0;	// 4 bits for the 4 batteries (if they hit 1.6v)

unsigned short adc_sum;				// Temp variable for the sum of 16 ADC's





ISR(ADC_vect)
{
	// Dummy, for ADC Sleep
}
/*
ISR(TIMER2_COMPA_vect)	// TIMER0 compare 8 bit
{
	PORTB ^= 0x8;
}
*/
ISR(TIMER1_COMPA_vect)	// TIMER1 compare 16 bit
{
	PORTC = 0;
	if (which_pulse == 1)	// Turn on the power/current for a fraction of a second
	{
		if (battery_charging & 1)	// Battery 1   (hmmm, shouldn't it be &&)
		{
			PORTC = 1;
		}
		if (battery_charging & 4)	// Battery 3
		{
			PORTC |= 4;
		}
	}
	else	// which_pulse == 0
	{
		if (battery_charging & 2)	// Battery 2
		{
			PORTC = 2;
		}
		if (battery_charging & 8)	// Battery 4
		{
			PORTC |= 8;
		}
	}
	which_pulse ^= 1;
}








int main(void)
{
	unsigned char i;	// a Temporary loop counter
	
	// Enable Outputs
	DDRB  = 0x3E;	// 5 Bar Leds
	DDRC  = 0xF;	// Power Pulses
	DDRD  = 0x3C;	// 4 Leds, Battery Number
	PORTC = 0;		// Power's off
	
	
	// Quick Led Test for 1 second
	PORTB = 0;	// 5 Leds
	PORTD = 0;	// 4 Leds
	_delay_ms(1000);
	
	
	// Serial Port Init
	UBRR0  = serial_port_divider;
	UCSR0B = (1 << TXEN0);					// Enable TX only
	UCSR0C = (0 << USBS0) | (3 << UCSZ00);	// 8,N,1
	
	
	// ADC Init
	set_sleep_mode(SLEEP_MODE_ADC);	// SLEEP_MODE_IDLE
	ADCSRA = (1<<ADEN) | (1<<ADIE) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);	// ADC Enable and prescaler of 128
	
	
	// Timers Init
	// TIMER0 used by delay() or similar
	
	/*	To delete, just playing
	OCR2A  = compair_match_register;
	TCCR2A = (1 << WGM21);				// CTC mode
	TCCR2B = (1 << CS22) | (1 << CS20);	// 1024 prescaler
	TIMSK2 = (1 << OCIE2A);				// enable timer compare interrupt
	TCNT2  = 0;
	*/
	
	OCR1A  = compair_match_register;
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);	// CTC mode & 1024 prescaler
	TIMSK1 = (1 << OCIE1A);								// enable timer compare interrupt
	TCNT1  = 0;
	
	sei();	// enable interrupts
	
	
	
	
	/*
	while(1)
	{
		TIMSK1 = 0;								// Temporarily disable timer1
		PORTC = 0xF;								// Add Charging Power
		checking_battery = 2;
		
		adc_sum = 0;
		for (i = 0; i < 16; i++)				// Get multiple samples to filter out some noise
		{
			ADMUX   = (3 << REFS0) | (checking_battery + 4);	// AREF = Internal 1.1v (0.99v) & Channel
			ADCSRA |= (1 << ADSC);				// Start single conversion
			sleep_mode();						// massively reduces noise
			while (ADCSRA & (1 << ADSC)) {};	// wait/check for conversion to complete �0'
			adc_sum += ADC;
		}
		adc_sum = adc_sum >> 4;
		
		
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = 0;
		_delay_ms(01);	// ooohhh, unfortunately it's needed on my board (cheap rejected crap)
		//while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		//UDR0 = checking_battery;
		//_delay_ms(10);
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = adc_sum >> 8;
		_delay_ms(10);
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = adc_sum;
		_delay_ms(500);
	}
	*/
	
	while(1)
	{
		TIMSK1 = 0;								// Temporarily disable timer1
		PORTC = 0;								// Remove Charging Power
		//_delay_ms(495);							// Should let it settle more
		
		// Check the Battery voltage
		adc_sum = 0;
		for (i = 0; i < 16; i++)				// Get multiple samples to filter out some noise
		{
			ADMUX   = (3 << REFS0) | (checking_battery + 4);	// AREF = Internal 1.1v (0.99v) & Channel
			ADCSRA |= (1 << ADSC);				// Start single conversion
			sleep_mode();						// massively reduces noise
			while (ADCSRA & (1 << ADSC)) {};	// wait/check for conversion to complete �0'
			adc_sum += ADC;
		}
		adc_sum = adc_sum >> 4;
		
		TIMSK1 = (1 << OCIE1A);					// Re-enable timer1
		
		
		
		PORTD = ~(1 << (checking_battery + 2));							// Show Battery Number Led
		PORTB = led1_bar | led2_bar | led3_bar | led4_bar | led5_bar;	// Clear the Bar Leds
		
		battery_charging &= ~(1 << checking_battery);					// Reset Charging Bit
		
		
		
		if (adc_sum <= volt_0000)		// No Battery / Almost Close Circuit
		{
			fully_charged &= ~(1 << checking_battery);			// Reset the battery flag
			//PORTB &= ~led3_bar;	// *** Test
			//_delay_ms(250);		// *** Test
		}
		else if (adc_sum < volt_0800)	// Bad Battery
		{
			fully_charged &= ~(1 << checking_battery);			// Reset the battery flag
			// Flash the Red Led
			for (i = 0; i < 6; i++)
			{
				PORTB ^= led1_bar;
				_delay_ms(250);
			}
		}
		else if ((adc_sum >= volt_1700)	||												// Battery is Fully Charged
		        ((adc_sum >  volt_1600) && (fully_charged && (1 << checking_battery))))	// Charged Battery? then Check > 1.5v
		{
			fully_charged |= (1 << checking_battery);			// Set the battery flag
			// Flash the Green Led
			for (i = 0; i < 6; i++)
			{
				PORTB ^= led5_bar;
				_delay_ms(250);
			}
		}
		else
		{
			battery_charging |= (1 << checking_battery);		// Set to charge battery
			fully_charged  &=  ~(1 << checking_battery);		// Reset Charged Bit
			PORTB = ~led1_bar;	// 0.8v
			if (adc_sum >= volt_1300)	{ PORTB &= ~led2_bar; }	// old = volt_0975
			if (adc_sum >= volt_1400)	{ PORTB &= ~led3_bar; }	// old = volt_1150
			if (adc_sum >= volt_1500)	{ PORTB &= ~led4_bar; }	// old = volt_1325
			if (adc_sum >= volt_1600)	{ PORTB &= ~led5_bar; }	// old = volt_1500
			_delay_ms(1500);
		}
		
		
		
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = 0;
		_delay_ms(1);	// ooohhh, unfortunately it's needed on my board (cheap rejected crap)
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = checking_battery;
		_delay_ms(1);
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = adc_sum >> 8;
		_delay_ms(1);
		while ( !( UCSR0A & (1 << UDRE0)) ) {};	// Wait till TX is Empty
		UDR0 = adc_sum;
		_delay_ms(1);
		
		
		
		// Change Places !!!
		checking_battery++;
		checking_battery %= 4;
	}
}
