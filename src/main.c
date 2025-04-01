#define F_CPU 16000000UL // 16MHz for Arduino Uno

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// DS1302 Pin Definitions
#define DS1302_RST PB2
#define DS1302_SCLK PB1
#define DS1302_IO PB0

// UART Functions
void uart_init()
{
    UBRR0L = 103; // 9600 baud for 16MHz
    UBRR0H = 0;
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data)
{
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = data;
}

unsigned char uart_receive()
{
    while (!(UCSR0A & (1 << RXC0)))
        ;
    return UDR0;
}

void uart_print(const char *str)
{
    while (*str)
        uart_transmit(*str++);
}

// DS1302 Functions
uint8_t decToBCD(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
uint8_t bcdToDec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

void ds1302_init()
{
    DDRB |= (1 << DS1302_RST) | (1 << DS1302_SCLK);
    PORTB &= ~((1 << DS1302_RST) | (1 << DS1302_SCLK));
}

void ds1302_start()
{
    PORTB |= (1 << DS1302_RST);
    _delay_us(4);
}

void ds1302_stop()
{
    PORTB &= ~(1 << DS1302_RST);
    _delay_us(4);
}

void ds1302_write_byte(uint8_t data)
{
    DDRB |= (1 << DS1302_IO);
    for (uint8_t i = 0; i < 8; i++)
    {
        if (data & 0x01)
            PORTB |= (1 << DS1302_IO);
        else
            PORTB &= ~(1 << DS1302_IO);

        PORTB |= (1 << DS1302_SCLK);
        _delay_us(1);
        PORTB &= ~(1 << DS1302_SCLK);
        _delay_us(1);
        data >>= 1;
    }
}

uint8_t ds1302_read_byte()
{
    uint8_t data = 0;
    DDRB &= ~(1 << DS1302_IO);

    for (uint8_t i = 0; i < 8; i++)
    {
        data >>= 1;
        if (PINB & (1 << DS1302_IO))
            data |= 0x80;

        PORTB |= (1 << DS1302_SCLK);
        _delay_us(1);
        PORTB &= ~(1 << DS1302_SCLK);
        _delay_us(1);
    }
    return data;
}

void ds1302_write_register(uint8_t addr, uint8_t data)
{
    ds1302_start();
    ds1302_write_byte(addr | 0x80);
    ds1302_write_byte(data);
    ds1302_stop();
}

uint8_t ds1302_read_register(uint8_t addr)
{
    ds1302_start();
    ds1302_write_byte(addr | 0x81);
    uint8_t data = ds1302_read_byte();
    ds1302_stop();
    return data;
}

void ds1302_set_time(uint8_t h, uint8_t m, uint8_t s)
{
    ds1302_write_register(0x80, decToBCD(s) & 0x7F);
    ds1302_write_register(0x82, decToBCD(m));
    ds1302_write_register(0x84, decToBCD(h));
}

void ds1302_get_time(uint8_t *h, uint8_t *m, uint8_t *s)
{
    *s = bcdToDec(ds1302_read_register(0x81) & 0x7F);
    *m = bcdToDec(ds1302_read_register(0x83));
    *h = bcdToDec(ds1302_read_register(0x85));
}

void ds1302_disable()
{
    // Set CH bit (bit7) to halt the clock
    uint8_t seconds = ds1302_read_register(0x81);
    ds1302_write_register(0x80, seconds | 0x80);
}

void ds1302_enable()
{
    // Clear CH bit to start the clock
    uint8_t seconds = ds1302_read_register(0x81);
    ds1302_write_register(0x80, seconds & 0x7F);
}

void print_menu()
{
    uart_print("\n\n----- RTC Control Menu -----\n");
    uart_print("1. Show Current Time\n");
    uart_print("2. Set New Time\n");
    uart_print("3. Disable Clock\n");
    uart_print("4. Enable Clock\n");
    uart_print("5. Exit Menu\n");
    uart_print("Enter choice (1-5): ");
}
void handle_menu()
{
    uint8_t choice = 0;
    uint8_t h, m, s;
    char input[20]; // Increased buffer size
    uint8_t i;
    char c;

    while (1)
    {
        print_menu();

        // Clear input buffer
        while (UCSR0A & (1 << RXC0))
            uart_receive();

        // Read menu choice
        i = 0;
        while (1)
        {
            while (!(UCSR0A & (1 << RXC0)))
                ; // Wait for input
            c = uart_receive();

            if (c == '\r' || c == '\n')
            {
                uart_print("\n");
                break;
            }

            if (i < sizeof(input) - 1)
            {
                input[i++] = c;
                uart_transmit(c); // Echo character
            }
        }
        input[i] = '\0';

        choice = atoi(input);

        switch (choice)
        {
        case 1: // Show time
            ds1302_get_time(&h, &m, &s);
            char time_str[20];
            snprintf(time_str, sizeof(time_str), "Current Time: %02d:%02d:%02d\n", h, m, s);
            uart_print(time_str);
            break;

        case 2: // Set time
            uart_print("Enter new time (HH MM SS): ");

            // Clear input buffer
            while (UCSR0A & (1 << RXC0))
                uart_receive();

            // Read time input
            i = 0;
            while (1)
            {
                while (!(UCSR0A & (1 << RXC0)))
                    ; // Wait for input
                c = uart_receive();

                if (c == '\r' || c == '\n')
                {
                    uart_print("\n");
                    break;
                }

                if (i < sizeof(input) - 1)
                {
                    input[i++] = c;
                    uart_transmit(c); // Echo character
                }
            }
            input[i] = '\0';

            if (sscanf(input, "%hhu %hhu %hhu", &h, &m, &s) == 3)
            {
                if (h < 24 && m < 60 && s < 60)
                {
                    ds1302_set_time(h, m, s);
                    uart_print("Time set successfully!\n");
                }
                else
                {
                    uart_print("Invalid time values! Use 24-hour format\n");
                }
            }
            else
            {
                uart_print("Invalid input format! Use HH MM SS\n");
            }
            break;

        case 3: // Disable clock
            ds1302_disable();
            uart_print("Clock disabled (halted)\n");
            break;

        case 4: // Enable clock
            ds1302_enable();
            uart_print("Clock enabled (running)\n");
            break;

        case 5: // Exit
            uart_print("Exiting menu...\n");
            return;

        default:
            uart_print("Invalid choice! Try again.\n");
            break;
        }
    }
}

int main()
{

    uart_init();
    ds1302_init();
    DDRB |= (1 << PB5); // LED on PB5

    // Initial time setting (uncomment to use)
    ds1302_set_time(12, 0, 0);
    uart_print("\nInitializing device...\n");
    uart_print("\nDS1302 RTC Controller Ready\n");
    uart_print("\nPress 'm' for menu items\n");

    while (1)
    {
        // Blink LED to show system is running
        PORTB ^= (1 << PB5);

        // Check for menu activation character ('m')
        if (UCSR0A & (1 << RXC0))
        {
            if (uart_receive() == 'm')
            {
                handle_menu();
            }
        }

        _delay_ms(1000);
    }
}