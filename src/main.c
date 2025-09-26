#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t byte;

/* hardware pin mapping */
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

/* global time counter (milliseconds) */
volatile unsigned long g_millis_count = 0;

/* ISR */
ISR(TIMER0_COMPA_vect)
{
    g_millis_count++;
}

/* geeft milliseconden sinds start */
unsigned long millis(void)
{
    unsigned long m;
    cli();
    m = g_millis_count;
    sei();
    return m;
}

static void init_timer0(void)
{
    TCCR0A = (1 << WGM01);
    OCR0A = 249;
    TIMSK0 = (1 << OCIE0A);
    TCCR0B = (1 << CS01) | (1 << CS00);
    sei();
}

/* button state variablen */
static int g_button_state1 = 1;
static int g_button_state2 = 1;
static int g_last_button_state1 = 1;
static int g_last_button_state2 = 1;

static int g_counter = 0;
static bool g_b_waiting_for_button2 = false;

static unsigned long g_time_button1 = 0;
static unsigned long g_time_button2 = 0;
static unsigned long g_delta_time = 0;

static const float g_distance_m = 0.6f; /* afstand telslangen */
static bool g_b_speed_valid = false;


static byte g_patterns[] =
{
    0b1111110, /* 0 */
    0b0110000, /* 1 */
    0b1101101, /* 2 */
    0b1111001, /* 3 */
    0b0110011, /* 4 */
    0b1011011, /* 5 */
    0b1011111, /* 6 */
    0b1110000, /* 7 */
    0b1111111, /* 8 */
    0b1111011, /* 9 */
    0b1001111, /* A */
    0b0011111  /* b */
};

/* display buffer (4 digits) */
static int g_display_buffer[4] = {0, 0, 0, 0};

static void show_binary(int value)
{
    if ((value & 0x01) != 0)
    {
        PORTD |= (1 << LED1);
    }
    else
    {
        PORTD &= ~(1 << LED1);
    }

    if ((value & 0x02) != 0)
    {
        PORTD |= (1 << LED2);
    }
    else
    {
        PORTD &= ~(1 << LED2);
    }

    if ((value & 0x04) != 0)
    {
        PORTD |= (1 << LED3);
    }
    else
    {
        PORTD &= ~(1 << LED3);
    }

    if ((value & 0x08) != 0)
    {
        PORTD |= (1 << LED4);
    }
    else
    {
        PORTD &= ~(1 << LED4);
    }
}

static void show_speed(float speed_kmh)
{
    int value = (int)(speed_kmh + 0.5f);

    if (value > 9999)
    {
        value = 9999;
    }

    if (value < 0)
    {
        value = 0;
    }

    for (int i = 0; i < 4; i++)
    {
        g_display_buffer[i] = value % 10;
        value /= 10;
    }
}

static void clear_display(void)
{
    for (int i = 0; i < 4; i++)
    {
        g_display_buffer[i] = 0;
    }
}

static void show_digit(int digit_index, int pattern_index)
{
    PORTC |= (1 << DIG1) | (1 << DIG2) | (1 << DIG3) | (1 << DIG4);

    byte pattern = g_patterns[pattern_index];

    for (int i = 0; i < 6; i++)
    {
        if ((pattern & (1 << (6 - i))) != 0)
        {
            PORTB |= (1 << i);
        }
        else
        {
            PORTB &= ~(1 << i);
        }
    }

    if ((pattern & 1) != 0)
    {
        PORTC |= (1 << SEG_G);
    }
    else
    {
        PORTC &= ~(1 << SEG_G);
    }

    switch (digit_index)
    {
        case 0: PORTC &= ~(1 << DIG1); break;
        case 1: PORTC &= ~(1 << DIG2); break;
        case 2: PORTC &= ~(1 << DIG3); break;
        case 3: PORTC &= ~(1 << DIG4); break;
        default: break;
    }

    _delay_ms(2);
}

static void initialize_io(void)
{

    DDRD &= ~((1 << BTN1) | (1 << BTN2));
    PORTD |= (1 << BTN1) | (1 << BTN2);

    DDRD |= (1 << LED1) | (1 << LED2) | (1 << LED3) | (1 << LED4);
    DDRB |= (1 << SEG_A) | (1 << SEG_B) | (1 << SEG_C) |
            (1 << SEG_D) | (1 << SEG_E) | (1 << SEG_F);
    DDRC |= (1 << SEG_G) | (1 << DIG1) | (1 << DIG2) |
            (1 << DIG3) | (1 << DIG4);

    clear_display();
}

int main(void)
{
    init_timer0();
    initialize_io();

    while (1)
    {
        for (int d = 0; d < 4; d++)
        {
            if (g_b_speed_valid)
            {
                show_digit(d, g_display_buffer[d]);
            }
            else
            {
                PORTC |= (1 << DIG1) | (1 << DIG2) | (1 << DIG3) | (1 << DIG4);
            }
        }

        g_button_state1 = ((PIND & (1 << BTN1)) != 0) ? 1 : 0;
        g_button_state2 = ((PIND & (1 << BTN2)) != 0) ? 1 : 0;

        if ((g_button_state1 == 0) && (g_last_button_state1 == 1))
        {
            g_b_waiting_for_button2 = true;
            g_time_button1 = millis();
        }

        if ((g_button_state2 == 0) && (g_last_button_state2 == 1) && g_b_waiting_for_button2)
        {
            g_time_button2 = millis();
            g_delta_time = g_time_button2 - g_time_button1;

            g_counter++;
            if (g_counter > 15)
            {
                g_counter = 0;
            }

            float time_s = g_delta_time / 1000.0f;
            float speed_kmh = (g_distance_m / time_s) * 3.6f;

            show_binary(g_counter);

            if (g_counter > 0)
            {
                show_speed(speed_kmh);
                g_b_speed_valid = true;
            }
            else
            {
                clear_display();
                g_b_speed_valid = false;
            }

            g_b_waiting_for_button2 = false;
        }

        g_last_button_state1 = g_button_state1;
        g_last_button_state2 = g_button_state2;

        _delay_ms(5);
    }
}