/* Autor: Ján Folenta (xfolen00)
 * originál
 * Poslendá zmena: 22.12.2019
 */

#include "MK60D10.h"

#define GPIO_PIN_MASK 0x1Fu
#define GPIO_PIN(x) (((1)<<(x & GPIO_PIN_MASK)))

// Namapovanie LEDiek a tlačidiel na príslušné piny portov
#define LED_D9  0x20      // Port B, bit 5
#define LED_D10 0x10      // Port B, bit 4
#define LED_D11 0x8       // Port B, bit 3
#define LED_D12 0x4       // Port B, bit 2

#define BTN_SW2 0x400     // Port E, bit 10
#define BTN_SW3 0x1000    // Port E, bit 12
#define BTN_SW4 0x8000000 // Port E, bit 27
#define BTN_SW5 0x4000000 // Port E, bit 26
#define BTN_SW6 0x800     // Port E, bit 11

#define SPK 0x10          // Speaker is on PTA4

#define DOT            500
#define DASH 		   10000
#define TIME_TO_DECODE 400000

long int pulldown_count = -1;
long long int time_out = 0;
char letter[5] = "";
int i = 0;

// Funckia podľa paramaetru bound nastaví delay
void delay(long long bound) {

  long long i;
  for(i = 0; i < bound; i++);
}

// Inicializácia MCU
void MCUInit(void)  {
    MCG_C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01) );
    SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);
    WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
}

// Inicializácia portov
void PortsInit(void)
{
	// Zapnutie hodín na všetkých portoch
    SIM->SCGC5 = SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTE_MASK | SIM_SCGC5_PORTA_MASK;
    SIM->SCGC1 = SIM_SCGC1_UART5_MASK;

    // Nastavenie príslušných PTB pinov pre funkcionalitu GPIO
    PORTB->PCR[5] = PORT_PCR_MUX(0x01); // D9
    PORTB->PCR[4] = PORT_PCR_MUX(0x01); // D10
    PORTB->PCR[3] = PORT_PCR_MUX(0x01); // D11
    PORTB->PCR[2] = PORT_PCR_MUX(0x01); // D12

    PORTE->PCR[8]  = PORT_PCR_MUX(0x03); // UART0_TX
    PORTE->PCR[9]  = PORT_PCR_MUX(0x03); // UART0_RX
    PORTE->PCR[10] = PORT_PCR_MUX(0x01); // SW2
    PORTE->PCR[12] = PORT_PCR_MUX(0x01); // SW3
    PORTE->PCR[27] = PORT_PCR_MUX(0x01); // SW4
    PORTE->PCR[26] = PORT_PCR_MUX(0x01); // SW5
    PORTE->PCR[11] = PORT_PCR_MUX(0x01); // SW6

    PORTA->PCR[4] = PORT_PCR_MUX(0x01);  // Speaker

    // Nastavenie príslušných PTB pinov portov ako výstupy
    PTB->PDDR = GPIO_PDDR_PDD(0x3C);     // LED porty ako výstup
    PTA->PDDR = GPIO_PDDR_PDD(SPK);      // Bzučiak ako výstup
    PTB->PDOR |= GPIO_PDOR_PDO(0x3C);    // Vypnutie všetkých LED
    PTA->PDOR &= GPIO_PDOR_PDO(~SPK);    // Bzučiak je ticho
}

// Inicializácia rozhrania UART
void UART5Init() {
    UART5->C2  &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);
    UART5->BDH =  0x00;
    UART5->BDL =  0x1A; // Baud rate 115 200 Bd, 1 stop bit
    UART5->C4  =  0x0F; // Oversampling ratio 16, match address mode disabled
    UART5->C1  =  0x00; // 8 data bitu, bez parity
    UART5->C3  =  0x00;
    UART5->MA1 =  0x00; // no match address (mode disabled in C4)
    UART5->MA2 =  0x00; // no match address (mode disabled in C4)
    UART5->S2  |= 0xC0;
    UART5->C2  |= ( UART_C2_TE_MASK | UART_C2_RE_MASK ); // Zapnout vysilac i prijimac
}

// Funkcia rozsvieti LEDky D9, D10, D11 a D12
void LEDS_on() {
	GPIOB_PDOR &= LED_D9;
	GPIOB_PDOR &= LED_D10;
	GPIOB_PDOR &= LED_D11;
	GPIOB_PDOR &= LED_D12;
}

// Funkcia zhasne LEDky D9, D10, D11 a D12
void LEDS_off() {
	GPIOB_PDOR ^= LED_D9;
	GPIOB_PDOR ^= LED_D10;
	GPIOB_PDOR ^= LED_D11;
	GPIOB_PDOR ^= LED_D12;
}

// Funkcia vyvolá pípnutie bzučiaku
void beep() {
	GPIOA_PDOR ^= SPK;
	delay(1000);
}

// Funckia vypíše znak na teminál
void SendCh(char c) {
    while( !(UART5->S1 & UART_S1_TDRE_MASK) && !(UART5->S1 & UART_S1_TC_MASK) );
    UART5->D = c;
}

// Funckia dekóduje zadanú skupinu symbolov Morseovej abecedy na znak latinky a daný znak vypíše na terminál
void PrintLetter(char *morseCode)
{
	if (strcmp(morseCode, ".-") == 0)
		SendCh('A');
	else if (strcmp(morseCode, "-...") == 0)
		SendCh('B');
	else if (strcmp(morseCode, "-.-.") == 0)
		SendCh('C');
	else if (strcmp(morseCode, "-..") == 0)
		SendCh('D');
	else if (strcmp(morseCode, ".") == 0)
		SendCh('E');
	else if (strcmp(morseCode, "..-.") == 0)
		SendCh('F');
	else if (strcmp(morseCode, "--.") == 0)
		SendCh('G');
	else if (strcmp(morseCode, "....") == 0)
		SendCh('H');
	else if (strcmp(morseCode, "..") == 0)
		SendCh('I');
	else if (strcmp(morseCode, ".---") == 0)
		SendCh('J');
	else if (strcmp(morseCode, "-.-") == 0)
		SendCh('K');
	else if (strcmp(morseCode, ".-..") == 0)
		SendCh('L');
	else if (strcmp(morseCode, "--") == 0)
		SendCh('M');
	else if (strcmp(morseCode, "-.") == 0)
		SendCh('N');
	else if (strcmp(morseCode, "---") == 0)
		SendCh('O');
	else if (strcmp(morseCode, ".--.") == 0)
		SendCh('P');
	else if (strcmp(morseCode, "--.-") == 0)
		SendCh('Q');
	else if (strcmp(morseCode, ".-.") == 0)
		SendCh('R');
	else if (strcmp(morseCode, "...") == 0)
		SendCh('S');
	else if (strcmp(morseCode, "-") == 0)
		SendCh('T');
	else if (strcmp(morseCode, "..-") == 0)
		SendCh('U');
	else if (strcmp(morseCode, "...-") == 0)
		SendCh('V');
	else if (strcmp(morseCode, ".--") == 0)
		SendCh('W');
	else if (strcmp(morseCode, "-..-") == 0)
		SendCh('X');
	else if (strcmp(morseCode, "-.--") == 0)
		SendCh('Y');
	else if (strcmp(morseCode, "--..") == 0)
		SendCh('Z');
}

int main(void)
{
    MCUInit();      // Inicializácia MCU
    PortsInit();    // Inicializácia portov
    UART5Init();	// Inicializace modulu UART5

    while (1) {

    	// Tlacidlo je stlacene
    	if (!(GPIOE_PDIR & BTN_SW6))
        {
        	pulldown_count++; // Inkrementuje sa počítadlo
        	beep(); // Rozobzučí sa bzučika
        	LEDS_on(); // Zasvietia sa LEDky
        }

        // Tlacidlo sa pusti
    	else if (GPIOE_PDIR & BTN_SW6 && pulldown_count > 0)
        {
    		LEDS_off(); // Thasnú sa LEDky
    		time_out = 0;

        	if (pulldown_count < DOT)
        	{
        		letter[i] = '.'; // Do poľa znakov sa priradí symbol '.'
        	}

        	else if (pulldown_count < DASH)
        	{
        		letter[i] = '-'; // Do poľa znakov sa priradí symbol ','
        	}

        	i++;
        	pulldown_count = -1; // Vynuluje sa počítadlo (nastaví sa na -1)
        }


        // Pokojny stav
    	else if (GPIOE_PDIR & BTN_SW6 && pulldown_count == -1)
    	{
    		time_out++; // Inkrementuje sa počítadlo

    		if (time_out == TIME_TO_DECODE)
    		{
    			if (strcmp(letter, "") != 0)
    			{
    				PrintLetter(letter); // Skupina symobolov sa dekóduje a vypíše na terminál
    				memset(letter,0,sizeof(letter)); // Vyprázdni sa pole symobolov
    				i = 0;
    			}

    			time_out = 0; // Vynuluje sa počítadlo
    		    i = 0;
    		}
    	}
    }

    return 0;
}
