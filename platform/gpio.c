/**
 * @file platform/gpio.c
 * @brief Platform-support routines, GPIO component + initialization entrypoints
 *
 * @author Alberto de Villa <alberto.de.villa@eee.upd.edu.ph>
 * @date   28 Oct 2024
 */

/*
 * PIC32CM5164LS00048 initial configuration:
 * -- Architecture: ARMv8 Cortex-M23
 * -- GCLK_GEN0: OSC16M @ 4 MHz, no additional prescaler
 * -- Main Clock: No additional prescaling (always uses GCLK_GEN0 as input)
 * -- Mode: Secure, NONSEC disabled
 * 
 * New clock configuration:
 * -- GCLK_GEN0: 24 MHz (DFLL48M [48 MHz], with /2 prescaler)
 * -- GCLK_GEN2: 4 MHz  (OSC16M @ 4 MHz, no additional prescaler)
 * 
 * HW configuration for the corresponding Curiosity Nano+ Touch Evaluation
 * Board:
 * -- PA15: Active-HI LED
 * -- PA23: Active-LO PB w/ external pull-up
 */

// Common include for the XC32 compiler
#include <xc.h>
#include <stdbool.h>
#include <string.h>
#include "blink_settings.h"

#include "clk.h"
#include "../platform.h"

int top = 23438;
// Initializers defined in other platform_*.c files
extern void platform_systick_init(void);
extern void platform_usart_init(void);
extern void platform_usart_tick_handler(const platform_timespec_t *tick);
/////////////////////////////////////////////////////////////////////////////



/*
 * Configure the EIC peripheral
 * 
 * NOTE: EIC initialization is split into "early" and "late" halves. This is
 *       because most settings within the peripheral cannot be modified while
 *       EIC is enabled.
 */
static void EIC_init_early(void) {
    /*
     * Enable the APB clock for this peripheral
     * 
     * NOTE: The chip resets with it enabled; hence, commented-out.
     * 
     * WARNING: Incorrect MCLK settings can cause system lockup that can
     *          only be rectified via a hardware reset/power-cycle.
     */
    // MCLK_REGS->MCLK_APBAMASK |= (1 << 10);

    /*
     * In order for debouncing to work, GCLK_EIC needs to be configured.
     * We can pluck this off GCLK_GEN2, configured for 4 MHz; then, for
     * mechanical inputs we slow it down to around 15.625 kHz. This
     * prescaling is OK for such inputs since debouncing is only employed
     * on inputs connected to mechanical switches, not on those coming from
     * other (electronic) circuits.
     * 
     * GCLK_EIC is at index 4; and Generator 2 is used.
     */
    GCLK_REGS->GCLK_PCHCTRL[4] = 0x00000042;
    while ((GCLK_REGS->GCLK_PCHCTRL[4] & 0x00000042) == 0)
        asm("nop");

    // Reset, and wait for said operation to complete.
    EIC_SEC_REGS->EIC_CTRLA = 0x01;
    while ((EIC_SEC_REGS->EIC_SYNCBUSY & 0x01) != 0)
        asm("nop");

    /*
     * Just set the debounce prescaler for now, and leave the EIC disabled.
     * This is because most settings are not editable while the peripheral
     * is enabled.
     */
    EIC_SEC_REGS->EIC_DPRESCALER = (0b0 << 16) | (0b0000 << 4) |
            (0b1111 << 0);
    return;
}

static void EIC_init_late(void) {
    /*
     * Enable the peripheral.
     * 
     * Once the peripheral is enabled, further configuration is almost
     * impossible.
     */
    EIC_SEC_REGS->EIC_CTRLA |= 0x02;
    while ((EIC_SEC_REGS->EIC_SYNCBUSY & 0x02) != 0)
        asm("nop");
    return;
}

// Configure the EVSYS peripheral

static void EVSYS_init(void) {
    /*
     * Enable the APB clock for this peripheral
     * 
     * NOTE: The chip resets with it enabled; hence, commented-out.
     * 
     * WARNING: Incorrect MCLK settings can cause system lockup that can
     *          only be rectified via a hardware reset/power-cycle.
     */
    //MCLK_REGS->MCLK_APBAMASK |= (1 << 0);

    /*
     * EVSYS is always enabled, but may be in an inconsistent state. As
     * such, trigger a reset.
     */
    EVSYS_SEC_REGS->EVSYS_CTRLA = 0x01;
    asm("nop");
    asm("nop");
    asm("nop");
    return;
}

//////////////////////////////////////////////////////////////////////////////

// Configure any peripheral/s used to effect blinking

/*
 * @brief Initializes PA 15 as the output LED with input enabled.  Active-Hi.
 * PA23 as input. Active-LO
 */
static void blink_init(void) {
    /*PA 15*/
    // DIR: 1; INEN: 1; PULLEN: X; OUT: X
    PORT_SEC_REGS->GROUP[0].PORT_DIRSET |= (1 << 15); // Set as output.
    // 31.7.14
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[15] |= (1 << 1); // Enables INEN 
    return;
}

int read_count() {
    // Allow read access of COUNT register
    // Return back the counter value
    TC0_REGS -> COUNT16.TC_CTRLBSET = (0x4 << 5);
    while (TC0_REGS -> COUNT16.TC_SYNCBUSY & (1 << 2));
    return TC0_REGS -> COUNT16.TC_COUNT; // 39.8.13

}

void platform_blink_modify(void) {
    // Start the timer if it's not running
    /*if (!(TC0_REGS->COUNT16.TC_STATUS & TC_STATUS_STOP_Msk)) {
        TC0_REGS->COUNT16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    }*/
    
    switch (currentSetting) {
        case OFF:
            PORT_SEC_REGS -> GROUP[0].PORT_OUTCLR = (1 << 15);
            break;
        case SLOW:
            TC0_REGS -> COUNT16.TC_CC[0] = 23438;
            while (TC0_REGS -> COUNT16.TC_SYNCBUSY & (1 << 6));
            if (read_count() < TC0_REGS -> COUNT16.TC_CC[0]*0.9) {
                PORT_SEC_REGS -> GROUP[0].PORT_OUTCLR = (1 << 15);
            }
            if (read_count() > TC0_REGS -> COUNT16.TC_CC[0]*0.9) {
                PORT_SEC_REGS -> GROUP[0].PORT_OUTSET = (1 << 15);
            }
            break;
        case MEDIUM:
            TC0_REGS -> COUNT16.TC_CC[0] = 11719;
            while (TC0_REGS -> COUNT16.TC_SYNCBUSY & (1 << 6));
            if (read_count() < TC0_REGS -> COUNT16.TC_CC[0]*0.8) {
                PORT_SEC_REGS -> GROUP[0].PORT_OUTCLR = (1 << 15);
            }
            if (read_count() > TC0_REGS -> COUNT16.TC_CC[0]*0.8) {
                PORT_SEC_REGS -> GROUP[0].PORT_OUTSET = (1 << 15);
            }
            break;
        case FAST:
            TC0_REGS -> COUNT16.TC_CC[0] = 7032;
            while (TC0_REGS -> COUNT16.TC_SYNCBUSY & (1 << 6));
            if (read_count() < TC0_REGS -> COUNT16.TC_CC[0]*0.5) {
                PORT_SEC_REGS -> GROUP[0].PORT_OUTCLR = (1 << 15);
            }
            if (read_count() > TC0_REGS -> COUNT16.TC_CC[0]*0.5) {
                PORT_SEC_REGS -> GROUP[0].PORT_OUTSET = (1 << 15);
            }
            break;
        case ON:
            PORT_SEC_REGS -> GROUP[0].PORT_OUTSET = (1 << 15);
            break;
    }

}
//////////////////////////////////////////////////////////////////////////////

/*
 * Per the datasheet for the PIC32CM5164LS00048, PA23 belongs to EXTINT[2],
 * which in turn is Peripheral Function A. The corresponding Interrupt ReQuest
 * (IRQ) handler is thus named EIC_EXTINT_2_Handler.
 */
static volatile uint16_t pb_press_mask = 0;

void __attribute__((used, interrupt())) EIC_EXTINT_2_Handler(void) {
    pb_press_mask &= ~PLATFORM_PB_ONBOARD_MASK;
    if ((EIC_SEC_REGS->EIC_PINSTATE & (1 << 2)) == 0)
        pb_press_mask |= PLATFORM_PB_ONBOARD_PRESS;
    else
        pb_press_mask |= PLATFORM_PB_ONBOARD_RELEASE;

    // Clear the interrupt before returning.
    EIC_SEC_REGS->EIC_INTFLAG |= (1 << 2);
    return;
}

static void PB_init(void) {
    /*
     * Configure PA23.
     * 
     * NOTE: PORT I/O configuration is never separable from the in-circuit
     *       wiring. Refer to the top of this source file for each PORT
     *       pin assignments.
     */
    /*
    PORT_SEC_REGS->GROUP[0].PORT_DIRCLR = 0x00800000;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[23] = 0x03;
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[(23 >> 1)] &= ~(0xF0);*/
    /*PA 23*/

    // 31.7.1
    PORT_SEC_REGS->GROUP[0].PORT_DIRCLR |= (1 << 23); // Set as input.
    // 31.7.14
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[23] |= 0x7; // Enables PULLEN, INEN, and PMUXEN, input with pull.
    // 31.7.6
    PORT_SEC_REGS->GROUP[0].PORT_OUTSET |= (1 << 0); // Set as internal pull-up.
    // 31.7.13
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[11] |= (0x0 << 4); // A Peripheral for PA23, PMUXO[3:0], required PMUXEN 1
    /*
     * Debounce EIC_EXT2, where PA23 is.
     * 
     * Configure the line for edge-detection only.
     * 
     * NOTE: EIC has been reset and pre-configured by the time this
     *       function is called.
     */
    EIC_SEC_REGS->EIC_DEBOUNCEN |= (1 << 2);
    EIC_SEC_REGS->EIC_CONFIG0 &= ~((uint32_t) (0xF) << 8);
    EIC_SEC_REGS->EIC_CONFIG0 |= ((uint32_t) (0xB) << 8);

    /*
     * NOTE: Even though interrupts are enabled here, global interrupts
     *       still need to be enabled via NVIC.
     */
    EIC_SEC_REGS->EIC_INTENSET = 0x00000004;
    return;
}

// Get the mask of currently-pressed buttons

uint16_t platform_pb_get_event(void) {
    uint16_t cache = pb_press_mask;

    pb_press_mask = 0;
    return cache;
}

//////////////////////////////////////////////////////////////////////////////

/*
 * Configure the NVIC
 * 
 * This must be called last, because interrupts are enabled as soon as
 * execution returns from this function.
 */
static void NVIC_init(void) {
    /*
     * Unlike AHB/APB peripherals, the NVIC is part of the Arm v8-M
     * architecture core proper. Hence, it is always enabled.
     */
    __DMB();
    __enable_irq();
    NVIC_SetPriority(EIC_EXTINT_2_IRQn, 3);
    NVIC_SetPriority(SysTick_IRQn, 3);
    NVIC_EnableIRQ(EIC_EXTINT_2_IRQn);
    NVIC_EnableIRQ(SysTick_IRQn);
    return;
}

/////////////////////////////////////////////////////////////////////////////

/* Configure the Emergency GPIO Pins 
 * PA 19 is configured as output.
 * PA 18 is configured as input at pull-down (DIR: 0 | INEN: 1 | PULLEN: 1 | OUT: 0)
 */
static void Emergency_Pins_Init(void){
    // For PA 19
    PORT_SEC_REGS -> GROUP[0].PORT_PINCFG[19] |= (1 << 1); // INEN: 1 | No interrupt needed.
    PORT_SEC_REGS -> GROUP[0].PORT_DIRSET |= (1 << 19); // Configure as output.
    PORT_SEC_REGS -> GROUP[0].PORT_OUTSET |= (1 << 19); // Configure as active-HI.
    
    // For PA 18
    PORT_SEC_REGS -> GROUP[0].PORT_PINCFG[18] |= (0x7 << 0); // INEN: 1 | PULLEN: 1 | PMUXEN: 1
    PORT_SEC_REGS -> GROUP[0].PORT_DIRSET |= (0 << 18); // Configure as input.
    PORT_SEC_REGS -> GROUP[0].PORT_OUTSET |= (0 << 18); // Configure as pull-down.
    PORT_SEC_REGS -> GROUP[0].PORT_PMUX[9] |= (0x0 << 0); // Enable the A Peripheral for EXTINT[7]
}   

// Initialize the platform

void platform_init(void) {
    // Raise the power level
    raise_perf_level();

    // Early initialization
    EVSYS_init();
    EIC_init_early();

    // Regular initialization
    TC0_Init();
    PB_init();
    Emergency_Pins_Init();
    blink_init();
    platform_usart_init();

    // Late initialization
    EIC_init_late();
    platform_systick_init();
    NVIC_init();
    return;
}



// Do a single event loop

void platform_do_loop_one(void) {
    platform_timespec_t tick;

    /*
     * Some routines must be serviced as quickly as is practicable. Do so
     * now.
     */
    platform_tick_hrcount(&tick);
    platform_usart_tick_handler(&tick);
}
