/* 
 * File:   main.c
 * Author: JRK
 *
 * Created on August 3, 2015, 7:36 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <xc.h>

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

// CONFIG
#pragma config FOSC = INTOSCIO  // Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA4/OSC2/CLKOUT pin, I/O function on RA5/OSC1/CLKIN)
#pragma config WDTE = ON        // Watchdog Timer Enable bit (WDT enabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select bit (MCLR pin function is digital input, MCLR internally tied to VDD)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown Out Detect (BOR enabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal External Switchover mode is enabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is enabled)

#define ENABLE GPIObits.GP1
#define LED2 GPIObits.GP4
#define LED3 GPIObits.GP5

uint16_t adcValue = 0;
uint8_t tempDegF = 0;
uint16_t fanSpeed = 0;

void SetupOscillator()
{
    OSCCONbits.IRCF = 0b00000110;
    OSCCONbits.OSTS = 0;
    OSCCONbits.SCS = 1;
    WDTCONbits.WDTPS = 0b00000101;

    while(!OSCCONbits.HTS) {}
}

void SetupPinIO()
{
    ANSELbits.ANS0 = 1;     // analog input
    ANSELbits.ANS1 = 0;
    ANSELbits.ANS2 = 0;
    ANSELbits.ANS3 = 0;
    TRISIObits.TRISIO0 = 1;     // analog input
    TRISIObits.TRISIO1 = 0;     // enable
    TRISIObits.TRISIO2 = 0;     // LED1 & PWM
    TRISIObits.TRISIO4 = 0;     // LED2
    TRISIObits.TRISIO5 = 0;     // LED3
}

void SetupADC()
{
    ANSELbits.ADCS = 0b00000110;    // Fosc/64
    ADCON0bits.ADFM = 1;            // Right Justify, 10bit ADC from ADRESH & ADRESL
    ADCON0bits.VCFG = 0;            // VDD +REF
    ADCON0bits.CHS = 0;
    ADCON0bits.ADON = 1;
}

void SetupPWM()
{
    T2CONbits.TOUTPS = 0;
    T2CONbits.T2CKPS = 0;
    T2CONbits.TMR2ON = 1;
    TRISIObits.TRISIO2 = 0;
    CCP1CONbits.CCP1M = 0b00001100;
    CCP1CONbits.DC1B = 0;
    CCPR1L = 0;
    PR2 = 0xFF;
}

uint16_t readADC()
{
    uint16_t result = 0;
    ADCON0bits.GO_DONE = 1;
    while(ADCON0bits.GO_DONE)
    {
        NOP();
    }
    result = ADRESL;
    result += (((uint16_t)ADRESH)<<8);
    
    return result;
}

uint8_t mapToDegF(const uint16_t adc)
{
    if(adc < 281)
    {
        return 245;
    }
    else if(adc > 837)
    {
        return 113;
    }
    else
    {
        uint16_t result = adc;
        result *= 15;   // (0.2337 coeff put into 6 fractional bits);  divide by 64 implicit from not shifting result<<6 to give it 6 fractional bits
        result *= -1;
        result += 19826;    // (309.78 * 2^6)
        result = (result>>6);
        return (uint8_t)result;
    }
}

uint16_t getFanSpeed(const uint8_t tempF)
{
    // fan speed 0-1023
    uint32_t fanSpeed;
    uint32_t temp = (uint16_t)tempF;
    if(tempF < 165)
        fanSpeed = 0;
    else if(tempF >= 165)
        fanSpeed = ((temp * 3836) / 100) - 6074;

    if(fanSpeed > 1023)
        fanSpeed = 1023;

    return (uint16_t)fanSpeed;
}

unsigned int delayLooper = 0;
void rampToSpeed(const unsigned int targetSpeed)
{
    uint16_t currentSpeed = (CCPR1L<<2) + CCP1CONbits.DC1B;
    uint16_t command = currentSpeed;

    ++delayLooper;
    if(delayLooper > 5)
    {
        delayLooper = 0;
        if(targetSpeed > currentSpeed)
            ++command;
        if(targetSpeed < currentSpeed)
            --command;
    }
    CCP1CONbits.DC1B = command;
    CCPR1L = (command>>2);
}

int main(int argc, char** argv)
{
    SetupOscillator();
    SetupPinIO();
    SetupPWM();
    SetupADC();

    LED2 = 1;
    LED3 = 1;
    ENABLE = 1;

    while(1)
    {
        CLRWDT();
        adcValue = readADC();
        tempDegF = mapToDegF(adcValue);
        fanSpeed = getFanSpeed(tempDegF);
        rampToSpeed(fanSpeed);
    }

    return (EXIT_SUCCESS);
}

