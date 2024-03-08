/*------------------------------------------------------------------------------
 * Initial Build::
 * File:            main.c
 * Description:
 *
 *
 * Input:
 * Output:
 * Author(s):   Polickoski, Nick
 * Date:        November 16, 2023
 *----------------------------------------------------------------------------*/

// Preprocessor Directives
#include <msp430xG46x.h>
#include "soundtrack.h"                                             // header file containing arrays of all 4 songs and their names
#include "symbols.h"                                                // header file for all string used


// Global Variables and Constants
volatile long int ADCx, ADCy;                                       // value: intermediate values taken from joy-stick
volatile float Xper, Yper;                                          // value: voltage percentages
volatile unsigned short int strike = 0;                             // counter: penalty counter
volatile unsigned int songIter = 0;                                 // counter: song iteration counter

char* songPtr = 0;                                                  // pointer for song selection (currently pointed to NULL)
int* songLenPtr = 0;                                                // pointer for currently selected song length

char arrowSent = 0;                                                 // flag - arrow sent: 0 - arrow in-progress, 1 - arrow sent
char endSong = 'p';                                                 // flag: p = song in-progress, w = end of song win, l = end of song lose



// Function Prototypes
void setupWDT(void);                                                // setup functions
void setupTimerA(void);                                             //
void setupADC(void);                                                //
void setupUART(void);                                               //

void UART_putCharacter(char c);                                     // UART shit
void UART_sendString(char* string);                                 //

void titleSequence(void);                                           // game-related functions
char directSelect(void);                                            //
void selectConfirm(char* string);                                   //
void clearScreen(void);                                             //
void restingState(void);                                            //
void endSongCondition(void);                                        //
char playAgain(void);                                               //
void arrowOutput(int iter);                                         //
void directConfirm(void);                                           //



//// Call to Main
void main(void)
{
    // Set up
    setupWDT();                                                     // Setup WDT
    setupTimerA();                                                  // Setup timer to send ADC data
    setupADC();                                                     // Setup ADC
    setupUART();                                                    // Setup UART

    _EINT();                                                        // enable global interrupts


    // Gameplay Loop
    char play = 'y';                                                // play again flag

    while (play == 'y')
    {
        titleSequence();
        clearScreen();


        // Song Loop
        while (endSong == 'p')
        {
            if (songIter == *songLenPtr)                            // If end-of-song reached
            {
                endSong = 'w';                                      // send win flag
                break;
            }

            IE1 |= WDTIE;                                           // turn on WDT interrupt
            while (arrowSent == 0);                                 // wait for arrow to be sent
            arrowSent = 0;                                          // reset sent arrow flag to False

            restingState();                                         // makes sure player has reset their thumbstick direction
            directConfirm();                                        // determines if player gets the point or not
            songIter++;                                             // every second iterate to the next song
        }


        // End of Song Conditions
        IE1 &= ~WDTIE;                                              // turn off WDT interrupt
        endSongCondition();


        // Reset Game Conditions
        songIter = 0;                                               // reset next direction in song array iterator
        strike = 0;                                                 // reset strike counter
        endSong = 'p';                                              // reset flag

        play = playAgain();                                         // play again sequence
    }

    clearScreen();

    return;
}



//// Interrupt Definitions
// ADC
#pragma vector = ADC12_VECTOR
__interrupt void ADC12ISR(void)
{
    ADCx = ADC12MEM0;                                               // Move results, IFG is cleared
    ADCy = ADC12MEM1;

    Xper = (ADCx * 3.0/4095 * 100/3);                               // Calculate percentage outputs
    Yper = (ADCy * 3.0/4095 * 100/3);

    __bic_SR_register_on_exit(LPM0_bits);                           // Exit LPM0

    return;
}


// Timer A
#pragma vector = TIMERA0_VECTOR
__interrupt void timerA_isr()
{
    // Control Register Getting New Values from Joystick
    ADC12CTL0 |= ADC12SC;                                           // Start conversions
    __bis_SR_register(LPM0_bits + GIE);                             // Enter LPM0

    return;
}


// WDT
#pragma vector = WDT_VECTOR
__interrupt void WDT_ISR()
{
    IFG1 &= ~WDTIFG;                                                // clear WDT interrupt flag

//    // Body of WDT Interrupt Goes Off Every 2 secs
//    static int count = 0;
//
//    if (count == 0)
//    {
//        count++;
//        return;
//    }
//
//    count = 0;

    arrowOutput(songIter);                                          // output correct song
    arrowSent = 1;                                                  // arrow sent flag to True

    return;
}



//// Function Definitions
// Setup Functions ---------------------
void setupWDT()
{
    WDTCTL = WDTPW | WDT_ADLY_1000;                 // set WDT to go off every 1 sec

    IE1 &= ~WDTIE;                                  // disable WDT interrupts (for now)
    IFG1 &= ~WDTIFG;                                // clear interrupt flag

    return;
}


void setupTimerA(void)
{
    TACCR0 = 3277;                                  // 3277 / 32768 Hz = 0.1s
    TACTL = TASSEL_1 + MC_1;                        // ACLK, up mode
    TACCTL0 = CCIE;                                 // Enabled interrupt

    return;
}


void setupADC(void)
{
    P6DIR &= ~BIT3 + ~BIT7;                         // Configure P6.3 and P6.7 as input pins
    P6SEL |= BIT3 + BIT7;                           // Configure P6.3 and P6.7 as analog pins

    ADC12CTL0 = ADC12ON + SHT0_6 + MSC;             // configure ADC converter
    ADC12CTL1 = SHP + CONSEQ_1;                     // Use sample timer, single sequence

    ADC12MCTL0 = INCH_3;                            // ADC A3 pin - Stick X-axis
    ADC12MCTL1 = INCH_7 + EOS;                      // ADC A7 pin - Stick Y-axis
                                                    // EOS - End of Sequence for Conversions
    ADC12IE |= 0x02;                                // Enable ADC12IFG.1

    int i = 0;
    for (i = 0; i < 0x3600; i++);                   // Delay for reference start-up

    ADC12CTL0 |= ENC;                               // Enable conversions

    return;
}


void setupUART(void)
{
    P2SEL |= BIT4 + BIT5;                           // Set up Rx and Tx bits
    UCA0CTL0 = 0;                                   // Set up default RS-232 protocol
    UCA0CTL1 |= BIT0 + UCSSEL_2;                    // Disable device, set clock

    UCA0BR0 = 9;                                    // 1048576 Hz / 115200
    UCA0BR1 = 0;
    UCA0MCTL = 0x02;

    UCA0CTL1 &= ~BIT0;                              // Start UART device

    return;
}


// UART Functions -------------------
void UART_putCharacter(char c)
{
    while(!(IFG2 & UCA0TXIFG));                     // Wait for previous character to be sent
    UCA0TXBUF = c;                                  // Send byte to the buffer for transmitting

    return;
}


void UART_sendString(char* string)
{
    int i;
    for (i = 0; string[i] != 0; i++)                // iterates through all characters in string and sends em
    {
        UART_putCharacter(string[i]);
    }

    return;
}


// Game Functions -------------------
void titleSequence(void)
{
    clearScreen();

    // Title
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString(title);
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);

    // Song Choosing Instruction
    UART_sendString(lineReset);
    UART_sendString(chooseInstr);
    UART_sendString(lineReset);

    // Song Decision Plus
    UART_sendString(lineReset);
    UART_sendString(songChoice1);
    UART_sendString(lineReset);
    UART_sendString(songChoice2);
    UART_sendString(lineReset);
    UART_sendString(songChoice3);
    UART_sendString(lineReset);

    // Ending Bar
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);

    restingState();


    // Direction Selection
    while (1)                                               // infinite loop in case no direction is output
    {
        char dir = directSelect();

        switch(dir)
        {
            // UP: song #1
            case 'U':
                songPtr = song1;
                songLenPtr = &song1Len;
                selectConfirm(song1Name);
                return;

            // DOWN: song #4
            case 'D':
                songPtr = song4;
                songLenPtr = &song4Len;
                selectConfirm(song4Name);
                return;

            // LEFT: song #2
            case 'L':
                songPtr = song2;
                songLenPtr = &song2Len;
                selectConfirm(song2Name);
                return;

            // RIGHT: song #3
            case 'R':
                songPtr = song3;
                songLenPtr = &song3Len;
                selectConfirm(song3Name);
                return;

            default:                                        // do nothing if no direction is output
                break;
        }
    }

}


char directSelect(void)
{
    /* Button Configuration Values
    *
    *              U
    *          L   +   R
    *              D
    */

    while (1)                                                       // loop until direction is chosen
    {
        // UP
        if ((Xper >= 35.0 && Xper <= 65.0) && (Yper >= 0.0 && Yper <= 15.0))
        {
            return 'U';
        }

        // DOWN
        if ((Xper >= 35.0 && Xper <= 65.0) && (Yper >= 85.0 && Yper <= 100.0))
        {
            return 'D';
        }

        // LEFT
        if ((Xper >= 0.0 && Xper <= 15.0) && (Yper >= 35.0 && Yper <= 65.0))
        {
            return 'L';
        }

        // RIGHT
        if ((Xper >= 85.0 && Xper <= 100.0) && (Yper >= 35.0 && Yper <= 65.0))
        {
            return 'R';
        }

//        // CENTER - error handling
//        if ((Xper <= 60.0 && Xper >= 40.0) && (Yper <= 60.0 && Yper >= 40.0))
//        {
//            return '_';
//        }
    }

}


void selectConfirm(char* string)
{
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString(" Is \"");
    UART_sendString(string);
    UART_sendString("\" your selection?");
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString("    Yes   +   No    ");
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);


    restingState();


    while (1)                                           // infinite loop in case L or R direction is not used
    {
        char dir = directSelect();

        switch (dir)
        {
            // LEFT: Yes
            case 'L':
                return;                                  // break out of song confirmation, song selection, and title sequence

            // RIGHT: No
            case 'R':
                clearScreen();                          // clear screen
                titleSequence();                        // replay title sequence again
                return;

            default:                                    // any other direction does nothing
                break;
        }
    }

}


void clearScreen(void)
{
    UART_sendString("\033[100A");                       // moves up lines
//    UART_sendString("\033[0D");                       // moves left lines
    UART_sendString("\033[2J");                       // uses clear screen character

    return;
}


void restingState(void)
{
    while (1)
    {
        // Waits for thumbstick to be at rest
        if ((Xper <= 60.0 && Xper >= 40.0) && (Yper <= 60.0 && Yper >= 40.0))
        {
            break;
        }
    }

    return;
}


void endSongCondition(void)
{
    if (endSong == 'w')
    {
        // Win Message
        UART_sendString(lineReset);
        UART_sendString(bar);
        UART_sendString(lineReset);
        UART_sendString(lineReset);
        UART_sendString(" You Won!! Congrats!!");
        UART_sendString(lineReset);
        UART_sendString(lineReset);
        UART_sendString(bar);
        UART_sendString(lineReset);
    }
    else if (endSong == 'l')
    {
        // Lose Message
        UART_sendString(lineReset);
        UART_sendString(bar);
        UART_sendString(lineReset);
        UART_sendString(lineReset);
        UART_sendString(" You lost :( Better Luck Next Time");
        UART_sendString(lineReset);
        UART_sendString(lineReset);
        UART_sendString(bar);
        UART_sendString(lineReset);
    }

    return;
}



char playAgain(void)
{
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString(" Play Again? ");
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString("    Yes   +   No    ");
    UART_sendString(lineReset);
    UART_sendString(lineReset);
    UART_sendString(bar);
    UART_sendString(lineReset);


    restingState();


    while (1)
    {
        char dir = directSelect();

        switch (dir)
        {
            // LEFT: Yes
            case 'L':
                return 'y';

            // RIGHT: No
            case 'R':
                return 'n';

            default:
                break;
        }
    }

}


void arrowOutput(int iter)
{
    clearScreen();

    switch (songPtr[iter])
    {
        // UP
        case 'U':
            UART_sendString(lineReset);
            UART_sendString(up);
            UART_sendString(lineReset);
            // buzz freq 1Hz
            return;

        // DOWN
        case 'D':
            UART_sendString(lineReset);
            UART_sendString(down);
            UART_sendString(lineReset);
            // buzz freq 2Hz
            return;

        // LEFT
        case 'L':
            UART_sendString(lineReset);
            UART_sendString(left);
            UART_sendString(lineReset);
            // buzz freq 3Hz
            return;

        // RIGHT
        case 'R':
            UART_sendString(lineReset);
            UART_sendString(right);
            UART_sendString(lineReset);
            // buzz freq 4Hz
            return;
    }

}


void directConfirm(void)
{
    char dir = directSelect();

    // debugging shit
    UART_putCharacter(dir);
    UART_sendString(lineReset);
    UART_putCharacter(songPtr[songIter]);


    if (dir == songPtr[songIter])                            // if correct direction chosen
    {
        UART_sendString(lineReset);
        UART_sendString(correct);
        UART_sendString(lineReset);
    }
    else if (dir != songPtr[songIter])                       // if incorrect/no direction chosen
    {
        UART_sendString(lineReset);
        UART_sendString(miss);
        UART_sendString(lineReset);

        strike++;                                            // increase strike counter

        if (strike >= 3)
        {
            endSong = 'l';                                   // send bad ending flag
        }
    }

    return;
}


