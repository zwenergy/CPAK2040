#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

// Pin Definitions.
#define A0 0
#define A1 1
#define A2 2
#define A3 3
#define A4 4
#define A5 5
#define A6 6
#define A7 7
#define A8 8
#define A9 9
#define A10 10
#define A11 11
#define A12 12
#define A13 13
#define A14 14
#define D0 15
#define D1 16
#define D2 17
#define D3 18
#define D4 19
#define D5 20
#define D6 21
#define D7 22
#define NCE 26
#define NOE 27
#define NWE 28

#define DATAOFFSET 15

#define LED_INT 25

#define FLASH_CS_PIN 1

// Bit masks
#define DATAMASK  0b00000011111111000000000000000
#define WRITEMASK 0b10000000000000000000000000000
#define CEMASK    0b00100000000000000000000000000
#define OEMASK    0b01000000000000000000000000000
#define CTRLMASK  0b11100000000000000000000000000

#define ADDRMASK  0b00000000000000111111111111111

#define MEMPAKSIZE 32768
#define NRMEMPAKS 12

// Address in the Flash for the first mem pak.
#define FLASHADDR ( PICO_FLASH_SIZE_BYTES - ( NRMEMPAKS * MEMPAKSIZE ) )

// Address in the Flash for the mempak counter.
#define MEMPAKCNTRADDR ( FLASHADDR - FLASH_SECTOR_SIZE )

// Idle time between checks of the listener.
#define REWRITECHECKINT_MS 500

// Waiting time after a write to the mem pak before the Flash is rewritten.
#define REWRITEWAIT_MS 5000

// Counter for checking mempak button change activate (multiples of REWRITECHECKINT_MS)
#define MEMPAKCHANGECNTMAX 4

// Blink pattern interval.
#define BLINKPATTERNINT_MS 250

// Time for the current mempak index to be stored as the current one to
// Flash (multiples of REWRITECHECKINT_MS).
#define MEMPAKCHANGESTOREINDEX (10000/REWRITECHECKINT_MS)

// Some function declarations.
void writeMemPakToFlash( uint8_t* pak );
void readMemPakFromFlash( uint8_t pak[] );
void writeMemPakCntrToFlash();
uint8_t readMemPakCntrFromFlash();
void rewriteFlashListener();
void doMemPakAction();
uint32_t getBootSelButton();
void doBlinkPattern( uint32_t count );

// The actual controller pak.
uint8_t mempak[ MEMPAKSIZE ];

// Flag variable to indicate re-write of the mem pak.
volatile uint32_t rewriteFlash = 0;

// Mempak counter
volatile uint8_t memPakCntr = 0;

void initGPIO() {
  // Set pin directions.
  gpio_init( A0 );
  gpio_set_dir( A0, GPIO_IN );
  gpio_init( A1 );
  gpio_set_dir( A1, GPIO_IN );
  gpio_init( A2 );
  gpio_set_dir( A2, GPIO_IN );
  gpio_init( A3 );
  gpio_set_dir( A3, GPIO_IN );
  gpio_init( A4 );
  gpio_set_dir( A4, GPIO_IN );
  gpio_init( A5 );
  gpio_set_dir( A5, GPIO_IN );
  gpio_init( A6 );
  gpio_set_dir( A6, GPIO_IN );
  gpio_init( A7 );
  gpio_set_dir( A7, GPIO_IN );
  gpio_init( A8 );
  gpio_set_dir( A8, GPIO_IN );
  gpio_init( A9 );
  gpio_set_dir( A9, GPIO_IN );
  gpio_init( A10 );
  gpio_set_dir( A10, GPIO_IN );
  gpio_init( A11 );
  gpio_set_dir( A11, GPIO_IN );
  gpio_init( A12 );
  gpio_set_dir( A12, GPIO_IN );
  gpio_init( A13 );
  gpio_set_dir( A13, GPIO_IN );
  gpio_init( A14 );
  gpio_set_dir( A14, GPIO_IN );
  
  gpio_init( NWE );
  gpio_set_dir( NWE, GPIO_IN );
  gpio_init( NCE );
  gpio_set_dir( NCE, GPIO_IN );
  gpio_init( NOE );
  gpio_set_dir( NOE, GPIO_IN );
  
  // Enable pull ups.
  gpio_set_pulls( NWE, true, false );
  gpio_set_pulls( NCE, true, false );
  gpio_set_pulls( NOE, true, false );
  
  // Initially, set the pins to IN
  gpio_init( D0 );
  gpio_set_dir( D0, GPIO_IN );
  gpio_init( D1 );
  gpio_set_dir( D1, GPIO_IN );
  gpio_init( D2 );
  gpio_set_dir( D2, GPIO_IN );
  gpio_init( D3 );
  gpio_set_dir( D3, GPIO_IN );
  gpio_init( D4 );
  gpio_set_dir( D4, GPIO_IN );
  gpio_init( D5 );
  gpio_set_dir( D5, GPIO_IN );
  gpio_init( D6 );
  gpio_set_dir( D6, GPIO_IN );
  gpio_init( D7 );
  gpio_set_dir( D7, GPIO_IN );
  
  gpio_init( LED_INT );
  gpio_set_dir( LED_INT, GPIO_OUT );
  
}

// The actual live mem pak handling.
void __not_in_flash_func( doMemPakAction() ) {
  
  while ( 1 ) {
    unsigned int data = gpio_get_all();
    unsigned int ctrl = ( data & CTRLMASK );
    
    unsigned int curAddr = data & ADDRMASK;
        
    switch ( ctrl )
    {
      case 0x0:
      case 0x8000000:
        // Write.
        // (Behaviour of the SRAM chip used in the controller. If /WE is
        // low, write mode is enabled, ignoring the state of /OE).
        gpio_set_dir_in_masked( DATAMASK );
        uint8_t writeByte = ( data & DATAMASK ) >> DATAOFFSET;
        mempak[ curAddr ] = writeByte;
        
        // Set flag.
        rewriteFlash = 1;
        break;
      case 0x10000000:
        // Read.
        gpio_set_dir_out_masked( DATAMASK );
        uint8_t membyte = mempak[ curAddr ];
        unsigned int gpiobyte = membyte << DATAOFFSET;
        gpio_put_all( gpiobyte );
        break;
      default:
        gpio_set_dir_in_masked( DATAMASK );
    }
  }
  
}

int __not_in_flash_func( main() ) {
  set_sys_clock_khz(250000, true);
  
  initGPIO();
  
  // Read out mem pak counter.
  memPakCntr = readMemPakCntrFromFlash();
  
  // If never saved, this will return 0xFF
  if ( memPakCntr > NRMEMPAKS - 1 ) {
    memPakCntr = 0;
  }
  
  // Read out old mem pak from Flash.
  readMemPakFromFlash( mempak );
  
  // Start the actual mem pak handling on the second core.
  multicore_launch_core1( doMemPakAction );
  
  // Signal which mem pak is active.
  doBlinkPattern( memPakCntr + 1 );
  
  // Wait a bit to continue, so that the second core is up and running,
  // because the "checking bootsel button" function called in the 
  // rewriteFlashListener will temporarily disable Flash access.
  sleep_ms( 100 );

  // Start a listener to rewrite flash in case a
  // modification happens (never returns).
  rewriteFlashListener();

  return 0;
}


void __not_in_flash_func( writeMemPakToFlash( uint8_t* pak ) ) {
  // Not interrupt-safe.
  uint32_t ints = save_and_disable_interrupts();
  
  uint32_t curMemPakAddr = FLASHADDR + ( memPakCntr * MEMPAKSIZE );

  // Bytes to be erased have to be a multiple of the sector size.
  // A mem pak is 32768 bytes large.
  // A flash sector is 4096 bytes.
  // So it's naturally a multiple.
  flash_range_erase( curMemPakAddr, MEMPAKSIZE );

  // And write.
  // Bytes to be erased have to be a multiple of the page size.
  // A mem pak is 32768 bytes large.
  // A flash page size is 256 bytes.
  // So it's naturally a multiple.
  flash_range_program( curMemPakAddr, pak, MEMPAKSIZE );
  
  // Restore interrupts.
  restore_interrupts ( ints );
}

void readMemPakFromFlash( uint8_t pak[] ) {
  // Offset addr after the RAM (XIP_BASE).
  uint8_t* addr = (uint8_t*) ( XIP_BASE + FLASHADDR + ( memPakCntr * MEMPAKSIZE ) );
  
  // Go over byte-wise.
  // TODO: This is stupid, just read out 32b chunks.
  for ( int i = 0; i < MEMPAKSIZE; ++i ) {
    pak[ i ] = *addr;
    ++addr;
  }
}

void __not_in_flash_func( writeMemPakCntrToFlash() ) {
  // Dummy array, because we need to write in multiples of 
  // FLASH_PAGE_SIZE.
  uint8_t dummyArray[ FLASH_PAGE_SIZE ];
  dummyArray[ 0 ] = memPakCntr;
  
  // Not interrupt-safe.
  uint32_t ints = save_and_disable_interrupts();

  // Bytes to be erased have to be a multiple of the sector size.
  flash_range_erase( MEMPAKCNTRADDR, FLASH_SECTOR_SIZE );

  // And write.
  flash_range_program( MEMPAKCNTRADDR, dummyArray, FLASH_PAGE_SIZE );
  
  // Restore interrupts.
  restore_interrupts ( ints );
}

uint8_t __not_in_flash_func( readMemPakCntrFromFlash() ) {
  // Offset addr after RAM.
  uint8_t* addr = (uint8_t*) ( XIP_BASE + MEMPAKCNTRADDR );
  uint8_t cntr = *addr;
  
  return cntr;
}

void __not_in_flash_func( rewriteFlashListener() ) {
  // This function basically just
  // waits for the Flash to be rewritten.
  // In order to avoid a lot of re-writes, we first wait a bit to make sure,
  // all mem pak writing is finished.
  
  // This function also listens to the bootsel button for changing the
  // virtual mem pak. We use the bootsel button, because we ran out of
  // regular GPIOs to use on a normal Pico board.
  uint32_t buttonPushCnt = 0;
  uint32_t changeDone = 0;
  uint32_t indexChangeCnt = -1;
  uint32_t indexStored = 1;
  
  while ( 1 ) {
    // Check if we should rewrite.
    if ( rewriteFlash ) {
      
      while ( 1 ) {
        // Toggle the flag.
        rewriteFlash = 0;
        
        gpio_put( LED_INT, 1 );
        
        // Wait a bit.
        sleep_ms( REWRITEWAIT_MS );
        
        // Did no new write happen?
        if ( !rewriteFlash ) {
          writeMemPakToFlash( mempak );
          rewriteFlash = 0;
          
          gpio_put( LED_INT, 0 );
          break;
        }
      }
    }
    
    
    // Check for button press.
    uint32_t curButton = getBootSelButton();

    if ( curButton ) {
      if ( buttonPushCnt >= MEMPAKCHANGECNTMAX && !changeDone ) {
        // Increase mem pak counter.
        if ( memPakCntr >= NRMEMPAKS - 1 ) {
          memPakCntr = 0;
        } else {
          ++memPakCntr;
        }
        
        // Reload mem pak.
        readMemPakFromFlash( mempak );

        doBlinkPattern( memPakCntr + 1 );
        
        indexChangeCnt = 0;
        indexStored = 0;
        changeDone = 1;
      }
      
      ++buttonPushCnt;
      
    } else {
      buttonPushCnt = 0;
      changeDone = 0;
    }
    
    if ( indexChangeCnt <= MEMPAKCHANGESTOREINDEX ) {
      ++indexChangeCnt;
      
    } else if ( !indexStored ) {
      // Write the current mem pak counter to flash.
      writeMemPakCntrToFlash( memPakCntr );
      indexStored = 1;
    }
    
    sleep_ms( REWRITECHECKINT_MS );
  }
}

// Very funny function to get the state of the bootsel button.
// Needs to be in RAM.
uint32_t __not_in_flash_func( getBootSelButton() ) {
  // Not interrupt-safe.
  uint32_t ints = save_and_disable_interrupts();
  
  // High-Z to Flash CS
  hw_write_masked( &ioqspi_hw->io[ FLASH_CS_PIN ].ctrl,
    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS );
    
  // Sleep function is in Flash (which is not working now), so we just
  // count up a bit.
  for ( volatile int32_t i = 0; i < 1000; ++i );
  
  // Read out the button state.
  uint32_t b = !( sio_hw->gpio_hi_in & ( 1u << FLASH_CS_PIN ) );

  // Enable the CS pin again.
  hw_write_masked( &ioqspi_hw->io[ FLASH_CS_PIN ].ctrl,
    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS );
  
  // Restore interrupts.
  restore_interrupts ( ints );
  
  return b;
}

void doBlinkPattern( uint32_t count ) {
  for ( uint32_t i = 0; i < count; ++i ) {
    gpio_put( LED_INT, 1 );
    sleep_ms( BLINKPATTERNINT_MS );
    gpio_put( LED_INT, 0 );
    sleep_ms( BLINKPATTERNINT_MS );
  }
}
