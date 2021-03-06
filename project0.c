//*****************************************************************************
//
// project0.c - Senior Design Software, ECE 49022 -- Purdue University, Smart Dog Door, Holden Yildirim
//
//
//*****************************************************************************

//Libraries

#include <tm4c123gh6pm.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_GPIO.h"
#include "inc/hw_uart.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/timer.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/uart.h"
#include "uartstdio.h"
#include "driverlib/ssi.h"

//*****************************************************************************
// Variable Declarations

//int drive = 0;      //flag to show that sensors and RFID are valid
int fwd1 = 0;       //flag to show that lock has passed first forward switch (slow down)
int fwd2 = 0;       //flag to show that lock has past second forward switch (kill-switch)
int back1 = 0;      //flag to show that lock has passed first backward switch (slow down)
int back2 = 0;      //flag to show that lock has passed second backward switch (kill-switch)
int i = 0;          //delay loop counter
char sensors = 'n'; // y or n indicating sensors in range
char rfid = 'n';    // y or n indicating sensors in range
int unlock = 0;
int lock = 0;



//*****************************************************************************
// Function Declarations

void PortEIntHandler(void);         //Port E interrupt handler
char readChar(void);                //UART character reader
void printChar(char c);             //UART character print
void printString(char * string);    //UART string print
char* readString(char delimiter);   //UART string reader


//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// Main 'C' Language entry point.  Toggle an LED using TivaWare.
//
//*****************************************************************************
int
main(void)
{
    //
    // Setup the system clock to run at 50 Mhz from PLL with crystal reference
    //
  //  SysCtlClockSet(SYSCTL_SYSDIV_4|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|
         //           SYSCTL_OSC_MAIN);

    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_OSC_INT | SYSCTL_USE_PLL);
    //PWM Initializations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF))
    {
    }
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);

    SysCtlPWMClockSet(SYSCTL_PWMDIV_2);  //25MHZ clock for PWM

    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;


    //PWM Configurations
    GPIOPinConfigure(GPIO_PF0_M1PWM4);                                                      //enable PF0 and PF1 for PWM
    GPIOPinConfigure(GPIO_PF1_M1PWM5);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    PWMGenConfigure(PWM1_BASE, PWM_GEN_2, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_2, 400);                                             //400 clock ticks = 125 khz
    PWMGenEnable(PWM1_BASE, PWM_GEN_2);                                                     //PWM enable

    //Switch Interrupt Initializations
    //Back1 switch
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE))
    {
    }
    GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0);
    GPIOPadConfigSet(GPIO_PORTE_BASE,GPIO_PIN_0,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU);   //Configure weak pull-up
    GPIOIntTypeSet(GPIO_PORTE_BASE,GPIO_PIN_0,GPIO_FALLING_EDGE);                           //Trigger interrupt on falling edge
    GPIOIntRegister(GPIO_PORTE_BASE,PortEIntHandler);                                       //Set handler function
    GPIOIntEnable(GPIO_PORTE_BASE, GPIO_INT_PIN_0);                                         //Enable interrupt
    //Back2 Switch
    GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_1);
    GPIOPadConfigSet(GPIO_PORTE_BASE,GPIO_PIN_1,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU);   //Configure weak pull-up
    GPIOIntTypeSet(GPIO_PORTE_BASE,GPIO_PIN_1,GPIO_FALLING_EDGE);                           //Trigger interrupt on falling edge
    GPIOIntRegister(GPIO_PORTE_BASE,PortEIntHandler);                                       //Set handler function
    GPIOIntEnable(GPIO_PORTE_BASE, GPIO_INT_PIN_1);                                         //Enable interrupt
    //Forward1 Switch
    GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTE_BASE,GPIO_PIN_2,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU);   //Configure weak pull-up
    GPIOIntTypeSet(GPIO_PORTE_BASE,GPIO_PIN_2,GPIO_FALLING_EDGE);                           //Trigger interrupt on falling edge
    GPIOIntRegister(GPIO_PORTE_BASE,PortEIntHandler);                                       //Set handler function
    GPIOIntEnable(GPIO_PORTE_BASE, GPIO_INT_PIN_2);                                         //Enable interrupt
    //Forward2 Switch
    GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_3);
    GPIOPadConfigSet(GPIO_PORTE_BASE,GPIO_PIN_3,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD_WPU);   //Configure weak pull-up
    GPIOIntTypeSet(GPIO_PORTE_BASE,GPIO_PIN_3,GPIO_FALLING_EDGE);                           //Trigger interrupt on falling edge
    GPIOIntRegister(GPIO_PORTE_BASE,PortEIntHandler);                                       //Set handler function
    GPIOIntEnable(GPIO_PORTE_BASE, GPIO_INT_PIN_3);                                         //Enable interrupt

    //UART Initializations
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);                                            //Enable GPIO Port A
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);                                            //Enable UART Module 0
    GPIOPinConfigure(GPIO_PB0_U1RX);                                                        //MUX UART to Port A pins 0 and 1
    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTClockSourceSet(UART1_BASE, UART_CLOCK_SYSTEM);                                       //Set UART clock
    UARTStdioConfig(1, 9600, SysCtlClockGet());

    UARTprintf("Welcome to the Dog Door Motor Control Prototype\n");

    // Loop Forever

    while(1)
    {
        if(unlock == 1)
        {
            if((back1 == 0) && (back2 == 0))
            {
                PWMPulseWidthSet(PWM1_BASE, PWM_OUT_4, 220);
                PWMOutputState(PWM1_BASE, PWM_OUT_4_BIT, true);     //enable unlock PWM
            }
            else if(back1 == 1)
            {
                PWMOutputState(PWM1_BASE, PWM_OUT_4_BIT, true);
                PWMPulseWidthSet(PWM1_BASE, PWM_OUT_4, 200);        //Reduce duty cycle
            }
            else if(back2 == 1)
            {
                PWMOutputState(PWM1_BASE, PWM_OUT_4_BIT, false);    //Shut off PWM1
                while(rfid != 'n')
                {
                    UARTprintf("Is the RFID still in range?(y/n)\n");
                    rfid = readChar();
                    if(rfid != 'n')
                    {
                        UARTprintf("The dog must move away from the door! Try again\n");
                    }
                }
                UARTprintf("Locking...\n");
                PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, 220);        //Set PWM2 to full speed
                PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, true);     //Turn on PWM2 (lock)
                unlock = 0;
                lock = 1;
                back2 = 0;
            }
        }
        else if(lock == 1)
        {
            if((fwd1 == 0) && (fwd2 == 0))
            {
                PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, true);
                PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, 220);        //Keep motor at full speed
            }
            else if(fwd1 == 1)
            {
                PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, true);
                PWMPulseWidthSet(PWM1_BASE, PWM_OUT_5, 200);        //Reduce duty cycle
            }
            else if(fwd2 == 1)
            {
                //PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, false);
                lock = 0;
                fwd2 = 0;

            }
        }
        else
        {
            PWMOutputState(PWM1_BASE, PWM_OUT_5_BIT, false);
            sensors = 'n';
            rfid = 'n';
            UARTprintf("Are all sensors within range?(y/n)\n\r");
            sensors = readChar();
            if(sensors == 'y')
            {
                UARTprintf("Is RFID tag in range?(y/n)\n\r");
                rfid = readChar();
                if(rfid == 'y')
                {
                    unlock = 1;
                    fwd1 = 0;
                    fwd2 = 0;
                    back1 = 0;
                    back2 = 0;
                    UARTprintf("Unlocking...\n");
                }
                else
                {
                    UARTprintf("Invalid Input\n\r");
                }
            }
            else
            {
                UARTprintf("Invalid Input\n\r");
            }
        }
    }
}

//*****************************************************************************
//
// Port E Interrupt Handler
//
//*****************************************************************************

void PortEIntHandler(void)
{
    uint32_t    status = 0;                             //Status variable to determine which pin triggered interrupt
    status = GPIOIntStatus(GPIO_PORTE_BASE,true);       //More pins will be added for interrupts
    GPIOIntClear(GPIO_PORTE_BASE,status);
    if( ((status & GPIO_INT_PIN_0) == GPIO_INT_PIN_0) && unlock)
    {
        //Then there was a pin0 interrupt, reduce back speed
        back1 = 1;
        back2 = 0;
        fwd1 = 0;
        fwd2 = 0;
    }
    else if( ((status & GPIO_INT_PIN_1) == GPIO_INT_PIN_1) && unlock)
    {
        //Then there was a pin1 interrupt, stop motor, delay, turn forward
        back2 = 1;
        back1 = 0;
        fwd1 = 0;
        fwd2 = 0;
    }
    else if( ((status & GPIO_INT_PIN_2) == GPIO_INT_PIN_2) && lock)
    {
        //Then there was a pin2 interrupt, reduce forward speed
        back2 = 0;
        back1 = 0;
        fwd1 = 1;
        fwd2 = 0;
    }
    else if( ((status & GPIO_INT_PIN_3) == GPIO_INT_PIN_3) && lock)
    {
        //Then there was a pin3 interrupt, stop motor
        back2 = 0;
        back1 = 0;
        fwd1 = 0;
        fwd2 = 1;
    }

}

//*****************************************************************************
//
// UART Print Character Function, credit:AllAboutEE
//
//*****************************************************************************

void printChar(char c)
{
    while((UART0_FR_R & (1<<5)) != 0);
    UART0_DR_R = c;
}

//*****************************************************************************
//
// UART Print String Function, credit:AllAboutEE
//
//*****************************************************************************

void printString(char * string)
{
  while(*string)
  {
    printChar(*(string++));
  }
}

//*****************************************************************************
//
// UART Read Character Function, credit:AllAboutEE
//
//*****************************************************************************


char readChar(void)
{
    char c;
    while((UART1_FR_R & (1<<4)) != 0);
    c = UART1_DR_R;
    return c;
}

//*****************************************************************************
//
// UART Read String Function, credit:AllAboutEE
//
//*****************************************************************************

char* readString(char delimiter)
{

  int stringSize = 0;
  char* string = (char*)calloc(10,sizeof(char));
  char c = readChar();
  printChar(c);

  while(c!=delimiter)
  {

    *(string+stringSize) = c; // put the new character at the end of the array
    stringSize++;

    if((stringSize%10) == 0) // string length has reached multiple of 10
    {
      string = (char*)realloc(string,(stringSize+10)*sizeof(char)); // adjust string size by increasing size by another 10
    }

    c = readChar();
    printChar(c); // display the character the user typed
  }

  if(stringSize == 0)
  {

    return '\0'; // null car
  }
  return string;
}
