#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t byte;

#define BTN1    PD2
#define BTN2    PD3
#define LED1    PD4
#define LED2    PD5
#define LED3    PD6
#define LED4    PD7
#define SEG_A   PB0
#define SEG_B   PB1
#define SEG_C   PB2
#define SEG_D   PB3
#define SEG_E   PB4
#define SEG_F   PB5
#define SEG_G   PC5
#define DIG1    PC1
#define DIG2    PC2
#define DIG3    PC3
#define DIG4    PC4


// Timer voor milliseconden
volatile unsigned long millis_count = 0;
ISR(TIMER0_COMPA_vect) { millis_count++; }


unsigned long millis(void) {
    unsigned long m;
    cli(); m = millis_count; sei();
    return m;
}
void init_timer0(void) {
    TCCR0A = (1<<WGM01);
    OCR0A = 249;
    TIMSK0 = (1<<OCIE0A);
    TCCR0B = (1<<CS01)|(1<<CS00);
    sei();
}


// Variabelen voor knoptoestand
int buttonState1 = 1;
int buttonState2 = 1;
int lastButtonState1 = 1;
int lastButtonState2 = 1;


// Meetvariabelen
int counter = 0;
bool waitingForButton2 = false;

unsigned long timeButton1 = 0;
unsigned long timeButton2 = 0;
unsigned long deltaTime = 0;

const float afstand = 0.6; //Afstand in meters tussen telslangen
bool snelheidGeldig = false;


byte patterns[] = {
  0b1111110, // 0
  0b0110000, // 1
  0b1101101, // 2
  0b1111001, // 3
  0b0110011, // 4
  0b1011011, // 5
  0b1011111, // 6
  0b1110000, // 7
  0b1111111, // 8
  0b1111011, // 9
  0b1001111, // A
  0b0011111 // b
};

int displayBuffer[4] = {0,0,0,0}; // Waarden 4 digits

void showBinary(int value) {
  if (value & 0x01) PORTD |= (1<<LED1); else PORTD &= ~(1<<LED1);
  if (value & 0x02) PORTD |= (1<<LED2); else PORTD &= ~(1<<LED2);
  if (value & 0x04) PORTD |= (1<<LED3); else PORTD &= ~(1<<LED3);
  if (value & 0x08) PORTD |= (1<<LED4); else PORTD &= ~(1<<LED4);
}

void showSnelheid(float snelheid) {
  int value = (int)(snelheid + 0.5);
  if (value > 9999) value = 9999;
  if (value < 0) value = 0;
  for (int i=0;i<4;i++){ displayBuffer[i] = value%10; value/=10; }
}

void clearDisplay() { for(int i=0;i<4;i++) displayBuffer[i]=0; }

void showDigit(int digitIndex, int patternIndex) {
  PORTC |= (1<<DIG1)|(1<<DIG2)|(1<<DIG3)|(1<<DIG4);
  byte pattern = patterns[patternIndex];
  for (int i=0;i<6;i++){
    if (pattern & (1<<(6-i))) PORTB |= (1<<i);
    else PORTB &= ~(1<<i);
  }
  if (pattern & 1) PORTC |= (1<<SEG_G); else PORTC &= ~(1<<SEG_G);
  switch(digitIndex){
    case 0: PORTC &= ~(1<<DIG1); break;
    case 1: PORTC &= ~(1<<DIG2); break;
    case 2: PORTC &= ~(1<<DIG3); break;
    case 3: PORTC &= ~(1<<DIG4); break;
  }
  _delay_ms(2);
}

void InitializeIO(void) {
  DDRD &= ~((1<<BTN1)|(1<<BTN2));
  PORTD |= (1<<BTN1)|(1<<BTN2);
  DDRD |= (1<<LED1)|(1<<LED2)|(1<<LED3)|(1<<LED4);
  DDRB |= (1<<SEG_A)|(1<<SEG_B)|(1<<SEG_C)|(1<<SEG_D)|(1<<SEG_E)|(1<<SEG_F);
  DDRC |= (1<<SEG_G)|(1<<DIG1)|(1<<DIG2)|(1<<DIG3)|(1<<DIG4);
  clearDisplay();
}

int main(void) {
  init_timer0();
  InitializeIO();

  while(1) {
    // Refresh display
    for(int d=0; d<4; d++) {
      if (snelheidGeldig) showDigit(d, displayBuffer[d]);
      else PORTC |= (1<<DIG1)|(1<<DIG2)|(1<<DIG3)|(1<<DIG4);
    }

    // Knoppen uitlezen (actief laag door pull-up)
    buttonState1 = (PIND & (1<<BTN1)) ? 1 : 0;
    buttonState2 = (PIND & (1<<BTN2)) ? 1 : 0;

    // Eerste knop ingedrukt
    if (buttonState1==0 && lastButtonState1==1) {
      waitingForButton2 = true;
      timeButton1 = millis();
    }

    // Tweede knop ingedrukt
    if (buttonState2==0 && lastButtonState2==1 && waitingForButton2) {
      timeButton2 = millis();
      deltaTime = timeButton2 - timeButton1;      // Verschil in ms
      counter++; if(counter>15) counter=0;        // Teller loopt tot 15
      float tijdSec = deltaTime/1000.0;           // Tijd in seconden
      float snelheid = (afstand/tijdSec)*3.6;     // Snelheid in km/h
      showBinary(counter);                        // Teller in binaire leds
      if (counter>0){ showSnelheid(snelheid); snelheidGeldig=true; }
      else { clearDisplay(); snelheidGeldig=false; }
      waitingForButton2=false;
    }

    // Vorige knoptoestand bijwerken
    lastButtonState1 = buttonState1;
    lastButtonState2 = buttonState2;

    _delay_ms(5); // Kleine vertraging (debounce en CPU rust)
  }
}
