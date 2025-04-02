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
    DDRB &= ~(1 << DS1302_IO); // Initialize as input
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
    ds1302_write_byte(addr | 0x80); // Write command
    ds1302_write_byte(data);
    ds1302_stop();
}

uint8_t ds1302_read_register(uint8_t addr)
{
    ds1302_start();
    ds1302_write_byte(addr | 0x81); // Read command
    uint8_t data = ds1302_read_byte();
    ds1302_stop();
    return data;
}

void ds1302_set_time(uint8_t h, uint8_t m, uint8_t s)
{
    // Ensure clock is running by clearing CH bit (bit7)
    ds1302_write_register(0x80, decToBCD(s) & 0x7F);
    ds1302_write_register(0x82, decToBCD(m));
    ds1302_write_register(0x84, decToBCD(h));
}

void ds1302_set_date(uint8_t day, uint8_t month, uint8_t year)
{
    ds1302_write_register(0x86, decToBCD(day));    // Day (1-31)
    ds1302_write_register(0x88, decToBCD(month));  // Month (1-12)
    ds1302_write_register(0x8C, decToBCD(year));   // Year (00-99)
}

void ds1302_get_time(uint8_t *h, uint8_t *m, uint8_t *s)
{
    *s = bcdToDec(ds1302_read_register(0x81) & 0x7F);
    *m = bcdToDec(ds1302_read_register(0x83));
    *h = bcdToDec(ds1302_read_register(0x85));
}

void ds1302_get_date(uint8_t *day, uint8_t *month, uint8_t *year)
{
    *day = bcdToDec(ds1302_read_register(0x87));
    *month = bcdToDec(ds1302_read_register(0x89));
    *year = bcdToDec(ds1302_read_register(0x8D));
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
    uart_print("1. Show Current Date & Time\n");
    uart_print("2. Set New Time\n");
    uart_print("3. Set New Date\n");
    uart_print("4. Disable Clock\n");
    uart_print("5. Enable Clock\n");
    uart_print("6. Exit Menu\n");
    uart_print("Enter choice (1-6): ");
}

uint8_t read_number()
{
    char input[4] = {0};
    uint8_t i = 0;
    char c;

    while (1)
    {
        c = uart_receive();
        if (c == '\r' || c == '\n')
        {
            uart_print("\n");
            break;
        }
        if (i < sizeof(input) - 1 && c >= '0' && c <= '9')
        {
            input[i++] = c;
            uart_transmit(c);
        }
    }

    // If no input provided, return 0
    if (i == 0)
    {
        return 0;
    }

    return atoi(input);
}

void handle_menu()
{
    uint8_t choice = 0;
    uint8_t h, m, s, day, month, year;
    char buffer[30];

    while (1)
    {
        choice = read_number();

        if (choice)
        {
            switch (choice)
            {
            case 1: // Show date and time
                ds1302_get_time(&h, &m, &s);
                ds1302_get_date(&day, &month, &year);
                snprintf(buffer, sizeof(buffer), "Date: %02d/%02d/20%02d\n", day, month, year);
                uart_print(buffer);
                snprintf(buffer, sizeof(buffer), "Time: %02d:%02d:%02d\n", h, m, s);
                uart_print(buffer);
                print_menu();
                break;

            case 2: // Set time
                uart_print("Enter hour (0-23): ");
                h = 0;
                while (h == 0)
                {
                    h = read_number();
                }

                uart_print("Enter minute (0-59): ");
                m = 0;
                while (m == 0)
                {
                    m = read_number();
                }

                uart_print("Enter second (0-59): ");
                s = 0;
                while (s == 0)
                {
                    s = read_number();
                }

                if (h < 24 && m < 60 && s < 60)
                {
                    ds1302_set_time(h, m, s);
                    uart_print("\r\nTime set successfully!\n");
                }
                else
                {
                    uart_print("Invalid time values! Use 24-hour format\n");
                }
                print_menu();
                break;

            case 3: // Set date
                uart_print("Enter day (1-31): ");
                day = 0;
                while (day == 0)
                {
                    day = read_number();
                }

                uart_print("Enter month (1-12): ");
                month = 0;
                while (month == 0)
                {
                    month = read_number();
                }

                uart_print("Enter year (00-99): ");
                year = 0;
                while (year == 0)
                {
                    year = read_number();
                }

                if (day > 0 && day < 32 && month > 0 && month < 13)
                {
                    ds1302_set_date(day, month, year);
                    uart_print("\r\nDate set successfully!\n");
                }
                else
                {
                    uart_print("Invalid date values!\n");
                }
                print_menu();
                break;

            case 4: // Disable clock
                ds1302_disable();
                uart_print("Clock disabled (halted)\n");
                print_menu();
                break;

            case 5: // Enable clock
                ds1302_enable();
                uart_print("Clock enabled (running)\n");
                print_menu();
                break;

            case 6: // Exit
                uart_print("Exiting menu...\n");
                return;

            default:
                uart_print("Invalid choice! Try again.\n");
                print_menu();
                break;
            }
        }
    }
}

int main()
{
    uart_init();
    ds1302_init();
    DDRB |= (1 << PB5); // LED on PB5

    // Ensure clock is running at startup
    uint8_t seconds = ds1302_read_register(0x81);
    ds1302_write_register(0x80, seconds & 0x7F); // Clear CH bit

    uart_print("\nInitializing device...\n");
    uart_print("DS1302 RTC Controller Ready\n");
    uart_print("Press 'm' for menu items\n");

    while (1)
    {
        // Blink LED to show system is running
        PORTB ^= (1 << PB5);

        // Check for menu activation character ('m')
        if (UCSR0A & (1 << RXC0))
        {
            if (uart_receive() == 'm')
            {
                print_menu();
                handle_menu();
            }
            else
            {
                uart_print("Press 'm' for menu items\n");
            }
        }

        _delay_ms(1000);
    }
}