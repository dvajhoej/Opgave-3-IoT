
/**
 * @file main.c
 * @brief Embedded Opgave 3 - Pointer og Bit Operationer
 * @target ATmega328PB Xplained Mini
 */

#define F_CPU 16000000UL

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

// -----------------------------------------
// Tilstandsmaskine
// -----------------------------------------

typedef enum { MODTAG_ADRESSE, MODTAG_BIT_POSITION, MODTAG_BIT_VAERDI } State;

// -----------------------------------------
// Globale UART-variabler
// -----------------------------------------

#define RX_BUFFER_SIZE 64

volatile char rxBuffer[RX_BUFFER_SIZE];
volatile uint8_t rxHead = 0;
volatile uint8_t rxTail = 0;
volatile uint8_t rxOverflow = 0;

// -----------------------------------------
// UART0 initialisering
// -----------------------------------------

void UART0_Init(void) {
  // 9600 baud ved 16 MHz
  UBRR0 = 103;

  // Receiver, transmitter og RX interrupt
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

  // 8 databits, ingen paritet, 1 stopbit
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// -----------------------------------------
// UART0 modtagelsesinterrupt
// -----------------------------------------

ISR(USART0_RX_vect) {
  char receivedChar = UDR0;

  uint8_t nextHead = (rxHead + 1) % RX_BUFFER_SIZE;

  if (nextHead != rxTail) {
    rxBuffer[rxHead] = receivedChar;
    rxHead = nextHead;
  } else {
    rxOverflow = 1;
  }
}

// -----------------------------------------
// Hent karakter fra UART-buffer
// -----------------------------------------

uint8_t UART0_Read(char *receivedChar) {
  if (rxHead == rxTail) {
    return 0;
  }

  *receivedChar = rxBuffer[rxTail];
  rxTail = (rxTail + 1) % RX_BUFFER_SIZE;

  return 1;
}

// -----------------------------------------
// Send karakter via UART0
// -----------------------------------------

void UART0_SendChar(char c) {
  while (!(UCSR0A & (1 << UDRE0))) {
  }

  UDR0 = c;
}

// -----------------------------------------
// Send tekst via UART0
// -----------------------------------------

void UART0_SendString(const char *str) {
  while (*str != '\0') {
    UART0_SendChar(*str);
    str++;
  }
}

// -----------------------------------------
// Send et tal i decimalformat
// -----------------------------------------

void UART0_SendNumber(uint8_t number) {
  if (number >= 10) {
    UART0_SendChar('0' + number / 10);
  }

  UART0_SendChar('0' + number % 10);
}

// -----------------------------------------
// Send hexadecimal adresse
// -----------------------------------------

void UART0_SendHex(uint16_t value) {
  const char hexDigits[] = "0123456789ABCDEF";

  UART0_SendString("0x");

  for (int8_t i = 12; i >= 0; i -= 4) {
    UART0_SendChar(hexDigits[(value >> i) & 0x0F]);
  }
}

// -----------------------------------------
// Send registervaerdi som binaert tal
// -----------------------------------------

void UART0_SendBinary(uint8_t value) {
  for (int8_t i = 7; i >= 0; i--) {
    if (value & (1 << i)) {
      UART0_SendChar('1');
    } else {
      UART0_SendChar('0');
    }

    if (i == 4) {
      UART0_SendChar(' ');
    }
  }

  UART0_SendString("\r\n");
}

// -----------------------------------------
// Konverter hexadecimal karakter
// -----------------------------------------

int8_t HexToNumber(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }

  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }

  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }

  return -1;
}

// -----------------------------------------
// Tilladte registeradresser
// -----------------------------------------

uint8_t IsValidAddress(uint16_t address) {
  switch (address) {
  case 0x0024: // DDRB
  case 0x0025: // PORTB
  case 0x0027: // DDRC
  case 0x0028: // PORTC
  case 0x002A: // DDRD
  case 0x002B: // PORTD
    return 1;

  default:
    return 0;
  }
}

// -----------------------------------------
// Udskriv aktuel tilstand
// -----------------------------------------

void PrintState(State state) {
  switch (state) {
  case MODTAG_ADRESSE:
    UART0_SendString("\r\nIndtast adresse (fx 0x0025:):\r\n");
    break;

  case MODTAG_BIT_POSITION:
    UART0_SendString("Indtast bitposition (0-7):\r\n");
    break;

  case MODTAG_BIT_VAERDI:
    UART0_SendString("Indtast bitvaerdi (0 eller 1):\r\n");
    break;
  }
}

// -----------------------------------------
// Main
// -----------------------------------------

int main(void) {

  State currentState = MODTAG_ADRESSE;

  uint16_t address = 0;
  uint8_t bitPosition = 0;
  uint8_t bitValue = 0;

  uint8_t addressDigits = 0;
  uint8_t prefixReceived = 0;
  uint8_t addressInvalid = 0;

  char receivedChar;

  // Pointer til 8-bit register
  volatile uint8_t *registerPointer;

  UART0_Init();
  sei();

  UART0_SendString("\r\nEmbedded Opgave 3\r\n");

  PrintState(currentState);

  while (1) {

    if (rxOverflow) {
      cli();
      rxOverflow = 0;
      rxTail = rxHead;
      sei();

      UART0_SendString("\r\nUART buffer overflow!\r\n");

      address = 0;
      addressDigits = 0;
      prefixReceived = 0;
      addressInvalid = 0;
      currentState = MODTAG_ADRESSE;

      PrintState(currentState);
      continue;
    }

    if (UART0_Read(&receivedChar)) {

      // Ignorer linjeskift
      if (receivedChar == '\r' || receivedChar == '\n') {
        continue;
      }

      switch (currentState) {

        // -------------------------
        // MODTAG ADRESSE
        // -------------------------

      case MODTAG_ADRESSE:

        if (receivedChar == ':') {

          if (addressDigits == 0 || addressInvalid ||
              !IsValidAddress(address)) {

            UART0_SendString("Ugyldig adresse!\r\n");

            address = 0;
            addressDigits = 0;
            prefixReceived = 0;
            addressInvalid = 0;

            PrintState(currentState);
            break;
          }

          currentState = MODTAG_BIT_POSITION;
          PrintState(currentState);
          break;
        }

        // Valgfrit 0x prefix
        if ((receivedChar == 'x' || receivedChar == 'X') &&
            addressDigits == 1 && address == 0 && !prefixReceived) {

          prefixReceived = 1;
          addressDigits = 0;
          break;
        }

        int8_t hexValue = HexToNumber(receivedChar);

        if (hexValue >= 0 && addressDigits < 4 && !addressInvalid) {

          address = (address << 4) | (uint8_t)hexValue;

          addressDigits++;

        } else {
          addressInvalid = 1;
        }

        break;

        // -------------------------
        // MODTAG BITPOSITION
        // -------------------------

      case MODTAG_BIT_POSITION:

        if (receivedChar >= '0' && receivedChar <= '7') {

          bitPosition = receivedChar - '0';

          currentState = MODTAG_BIT_VAERDI;
          PrintState(currentState);

        } else {

          UART0_SendString("Ugyldig bitposition!\r\n");

          currentState = MODTAG_ADRESSE;
          address = 0;
          addressDigits = 0;
          prefixReceived = 0;
          addressInvalid = 0;

          PrintState(currentState);
        }

        break;

        // -------------------------
        // MODTAG BITVAERDI
        // -------------------------

      case MODTAG_BIT_VAERDI:

        if (receivedChar == '0' || receivedChar == '1') {

          bitValue = receivedChar - '0';

          // Pointer til registeradresse
          registerPointer = (volatile uint8_t *)(uintptr_t)address;

          // Aendr kun den valgte bit
          if (bitValue == 1) {

            *registerPointer |= (1 << bitPosition);

          } else {

            *registerPointer &= ~(1 << bitPosition);
          }

          // Udskriv resultat
          UART0_SendString("\r\n--- RESULTAT ---\r\n");

          UART0_SendString("Adresse: ");
          UART0_SendHex(address);

          UART0_SendString("\r\nBitposition: ");
          UART0_SendNumber(bitPosition);

          UART0_SendString("\r\nBitvaerdi: ");
          UART0_SendNumber(bitValue);

          UART0_SendString("\r\nBitmoenster: ");
          UART0_SendBinary(*registerPointer);

          UART0_SendString("----------------\r\n");

        } else {

          UART0_SendString("Ugyldig bitvaerdi!\r\n");
        }

        // Tilbage til start
        currentState = MODTAG_ADRESSE;

        address = 0;
        addressDigits = 0;
        prefixReceived = 0;
        addressInvalid = 0;

        PrintState(currentState);

        break;
      }
    }
  }

  return 0;
}
