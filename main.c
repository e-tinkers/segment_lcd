/* This program demostrated how to drive a 4-digit segment LCD found in Aliexpress
 * The LCD has 4 COMs (COM1-COM4) and 8 multi-segment pins
 * The display has Drive Condition of: 1/4 Duty, 1/3 Bias, 3.3V
 * 
 * This code is tested on an ATtiny3227 running at 10MHz clock and 3.3V supply voltage.
 * 
 * Battery consumption for both ATtiny3227 and LCD is 3.5mA @10MHz clock without deep sleep
 * Battery consumption drop to 585uA with Power Down deep sleep implemented.
 * 
 * Contrast resistor used: 470 ohm. If the resistor value is too high, the LCD won't be able to turn on
 * 
 * // Pin Wiring
 * |-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
 * |LCD  | 1   | 2   | 3   | 4   | 12  | 11  | 10  | 9   | 8   | 7   | 6   | 5   |
 * |PIN  | PC3 | PC2 | PC1 | PC0 | PB7 | PB6 | PB5 | PB4 | PB3 | PB2 | PB1 | PB0 |
 * |COM  |COM4 |     |     |     | 4B  | 4A  | 3B  | 3A  | 2B  | 2A  | 1B  | 1A  |
 * |     |     |COM3 |     |     | 4G  | 4F  | 3G  | 3F  | 2G  | 2F  | 1G  | 1F  |
 * |     |     |     |COM2 |     | 4C  | 4E  | 3C  | 3E  | 2C  | 2E  | 1C  | 1E  |
 * |     |     |     |     |COM1 | 4D  | 4P  | 3D  | 3P  | 2D  | 2P  | 1D  | P1  |
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define COUNTUP_INTERVAL 100 // multiples of 2ms (interrupt occured at every 2ms, counterup in every 200ms)

volatile uint8_t refresh_ready = 1;

const char segment_table[] = {   // Segment Look up table (LUT)
           // | Segment | A      | B      | F      | G      | E      | C      | Dp     | D      |
           // |---------|--------|--------|--------|--------|--------|--------|--------|--------|
           // | Digit   | COM4-A | COM4-B | COM3-A | COM3-B | COM2-A | COM2-B | COM1-A | COM1-B |
    0xED,  // | "0"     | 1      |1       | 1      | 0      | 1      | 1      | 0      | 1      |
    0x44,  // | "1"     | 0      |1       | 0      | 0      | 0      | 1      | 0      | 0      |
    0xD9,  // | "2"     | 1      |1       | 0      | 1      | 1      | 0      | 0      | 1      |
    0xD5,  // | "3"     | 1      |1       | 0      | 1      | 0      | 1      | 0      | 1      |
    0x74,  // | "4"     | 0      |1       | 1      | 1      | 1      | 1      | 0      | 1      |
    0xB5,  // | "5"     | 1      |0       | 1      | 1      | 0      | 1      | 0      | 1      |
    0xBD,  // | "6"     | 1      |0       | 1      | 1      | 1      | 1      | 0      | 1      |
    0xC4,  // | "7"     | 1      |1       | 0      | 0      | 0      | 1      | 0      | 0      |
    0xFD,  // | "8"     | 1      |1       | 1      | 1      | 1      | 1      | 0      | 1      |
    0xF5   // | "9"     | 1      |1       | 1      | 1      | 0      | 1      | 0      | 1      |
};

uint8_t segment =  7;
uint8_t segs_out =  0;
// display counter initialized with "0000"
uint8_t LCD_d_1 =   0; // digit 1 as the left-most digit, and LCD_d_4 as the right-most digit
uint8_t LCD_d_2 =   0; // digit 10
uint8_t LCD_d_3 =   0; // digit 100
uint8_t LCD_d_4 =   0; // digit 1000

ISR (RTC_PIT_vect) {
    RTC.PITINTFLAGS = RTC_PI_bm;                      // Clear periodic interrupt flag
    refresh_ready = 1;                                // This is a flag for main loop
}

void pit_init () {
    while (RTC.STATUS > 0);                             // Wait until registers synchronized
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;                  // 32.768kHz Internal Oscillator
    RTC.PITCTRLA = RTC_PERIOD_CYC64_gc | RTC_PITEN_bm;  // 32768/64=512, i.e. 1/512=2ms between each interrupt
    RTC.PITINTCTRL = RTC_PI_bm;
}

void disableUnusedPin() {
    PORTA.DIRCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm | PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm;
    PORTC.DIRCLR = PIN4_bm | PIN5_bm;

    // disable input buffer to lower power consumption in sleep mode
    for (uint8_t i = 0; i < 8; i++) {
        *((uint8_t *)&PORTA + 0x10 + i) = PORT_ISC_INPUT_DISABLE_gc;
    }
    for (uint8_t i = 4; i < 6; i++) {
        *((uint8_t *)&PORTC + 0x10 + i) = PORT_ISC_INPUT_DISABLE_gc;;
    }
}

void refreshSegments() {
    segment++;

    if (segment > 7) {
        segment = 0;
    }

    // The following segment generates the 4 COM output waveforms via PORTC, each with HI and LOW outputs
    // PORTB for segment pins.
    switch (segment) {
        case 0:
            segs_out  = (segment_table[LCD_d_1] & 0x03);      // get digit_1's B & A bits
            segs_out |= (segment_table[LCD_d_2] & 0x03) << 2; // get digit_10's B & A bits
            segs_out |= (segment_table[LCD_d_3] & 0x03) << 4; // get digit_100's B & A bits
            segs_out |= (segment_table[LCD_d_4] & 0x03) << 6; // get digit_1000's B & A bits
            PORTC.DIR = 0;                                    // set all com pins to input
            PORTC.OUT = 0;
            PORTB.OUT = segs_out;
            PORTB.DIR = 0xFF;                                 // set all segment pin to output
            PORTC.DIRSET = PIN0_bm;                           // COM4 asserted LOW
            break;
        case 1:
            PORTC.OUTSET = PIN0_bm;
            PORTB.OUT = segs_out ^ 0xFF;                      // reverse segment outputs
            PORTB.DIR = 0xFF;                                 // set all segment pin to output
            PORTC.DIRSET = PIN0_bm;                           // COM4 asserted HIGH
            break;
        case 2:
            segs_out  = (segment_table[LCD_d_1] & 0x0c) >> 2; // get digit_1's G & F bits
            segs_out |= (segment_table[LCD_d_2] & 0x0c);      // get digit_10's G & F bits
            segs_out |= (segment_table[LCD_d_3] & 0x0c) << 2; // get digit_100's G & F bits
            segs_out |= (segment_table[LCD_d_4] & 0x0c) << 4; // get digit_1000's G & F bits
            PORTC.DIR = 0;
            PORTC.OUT = 0;
            PORTB.OUT = segs_out;
            PORTB.DIR = 0xFF;                                 // set all segment pin to output
            PORTC.DIRSET = PIN1_bm;                           // COM3 asserted LOW
            break;
        case 3:
            PORTC.OUT = PIN1_bm;
            PORTB.OUT = segs_out ^ 0xFF;                      // reverse segment outputs
            PORTB.DIR - 0xFF;
            PORTC.DIRSET = PIN1_bm;                           // COM3 asserted HIGH
            break;
        case 4:
            segs_out  = (segment_table[LCD_d_1] & 0x30) >> 4; // get digit_1's C & E bits
            segs_out |= (segment_table[LCD_d_2] & 0x30) >> 2; // get digit_10's C & E bits
            segs_out |= (segment_table[LCD_d_3] & 0x30);      // get digit_100's C & E bits
            segs_out |= (segment_table[LCD_d_4] & 0x30) << 2; // get digit_1000's C & E bits
            PORTC.DIR = 0;
            PORTC.OUT = 0;
            PORTB.OUT = segs_out;
            PORTB.DIR = 0xFF;
            PORTC.DIRSET = PIN2_bm;                           // COM2 asserted LOW
            break;
        case 5:
            PORTC.OUT = PIN2_bm;
            PORTB.OUT = segs_out ^ 0xFF;                      // reverse segment outputs
            PORTB.DIR = 0xFF;
            PORTC.DIRSET = PIN2_bm;                           // COM2 asserted HIGH
            break;
        case 6:
            segs_out  = (segment_table[LCD_d_1] & 0xC0) >> 6; // get digit_1000's DP & D bits
            segs_out |= (segment_table[LCD_d_2] & 0xC0) >> 4; // get digit_100's DP & D bits
            segs_out |= (segment_table[LCD_d_3] & 0xC0) >> 2; // get digit_10's DP & D bits
            segs_out |= (segment_table[LCD_d_4] & 0xC0);      // get digit_1's DP & D bits
            PORTC.DIR = 0;
            PORTC.OUT = 0;
            PORTB.OUT = segs_out;
            PORTB.DIR = 0xFF;
            PORTC.DIRSET = PIN3_bm;                           // COM1 asserted LOW
            break;
        case 7:
            PORTC.OUT = PIN3_bm;
            PORTB.OUT = segs_out ^ 0xFF;
            PORTB.DIR = 0xFF;
            PORTC.DIRSET = PIN3_bm;                           // COM1 asserted HIGH
            break;
        default:
            PORTB.DIRCLR = 0xFF;                              // clear all pins
            PORTC.DIRCLR = 0xFF;                              // COM1-COM4 float
    }

}

void main(void) {

    static uint16_t interval_counter =  0;

    _PROTECTED_WRITE(CLKCTRL_MCLKCTRLB, (CLKCTRL_PEN_bm | CLKCTRL_PDIV_2X_gc)); // running at 20/2 = 10MHz
    while (!(CLKCTRL.MCLKSTATUS & CLKCTRL_OSC20MS_bm)) {};
    // _delay_ms(100);

    disableUnusedPin();
    PORTB.DIR = 0xFF;         // set all PORTB(segments) pins as output
    PORTB.OUT = segs_out;     // set all segments to low
    PORTC.DIR = 0x00;         // set all PORTC(COMx) pins as input (high-impedance) for now

    pit_init();

    SLPCTRL.CTRLA = SLPCTRL_SMODE_PDOWN_gc;  // config sleep controller powerdown mode

    sei();

    // LCD refresh
    while (1) {
        if (refresh_ready) {

            refreshSegments();

            interval_counter++;                         // interval_counter increased in every 2ms
            if (interval_counter >= COUNTUP_INTERVAL) { // increment the counter every 2ms * COUNTUP_INTERVAL
                interval_counter = 0;

                LCD_d_1++;                           // 4-digit ripple BCD counter for LCD digits counting from 0000 to 9999
                if (LCD_d_1 >=10) {
                    LCD_d_1 = 0;
                    LCD_d_2++;
                }
                if (LCD_d_2 >=10) {
                    LCD_d_2 = 0;
                    LCD_d_3++;
                }
                if (LCD_d_3 >=10) {
                    LCD_d_3 = 0;
                    LCD_d_4++;
                }
                if (LCD_d_4 >=10) {
                    LCD_d_4 = 0;
                    LCD_d_1++;
                }
            }

            refresh_ready = 0;
        }
        SLPCTRL.CTRLA |= SLPCTRL_SEN_bm;  // sleep enable, this is equivalent to sleep_enable()
        __asm("sleep");                   // this is equivalent to sleep_cpu()
        SLPCTRL.CTRLA &= ~SLPCTRL_SEN_bm; // sleep disable , this is equivalent to sleep_disable()
    }

}
