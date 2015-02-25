/* Copyright (C) 2015 Goswin von Brederlow <goswin-v-b@web.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
 * UART on the Raspberry Pi
 */

#include "uart.h"
#include "peripherals.h"
#include "gpio.h"

namespace UART {
    enum {
	BASE = GPIO::GPIO_BASE + 0x1000, // 0x3F201000

	DR     = 0x00, // 0x3F201000
	RSRECR = 0x04, // 0x3F201004
	FR     = 0x18, // 0x3F201018
	ILPR   = 0x20, // 0x3F201020
	IBRD   = 0x24, // 0x3F201024
	FBRD   = 0x28, // 0x3F201028
	LCRH   = 0x2C, // 0x3F20102C
	CR     = 0x30, // 0x3F201030
	IFLS   = 0x34, // 0x3F201034
	IMSC   = 0x38, // 0x3F201038
	RIS    = 0x3C, // 0x3F20103C
	MIS    = 0x40, // 0x3F201040
	ICR    = 0x44, // 0x3F201044
	DMACR  = 0x48, // 0x3F201048
	ITCR   = 0x80, // 0x3F201080
	ITIP   = 0x84, // 0x3F201084
	ITOP   = 0x88, // 0x3F201088
	TDR    = 0x8C, // 0x3F20108C

	// Data register bits
	DR_OE = 1 << 11, // Overrun error
	DR_BE = 1 << 10, // Break error
	DR_PE = 1 <<  9, // Parity error
	DR_FE = 1 <<  8, // Framing error

	// Receive Status Register / Error Clear Register
	RSRECR_OE = 1 << 3, // Overrun error
	RSRECR_BE = 1 << 2, // Break error
	RSRECR_PE = 1 << 1, // Parity error
	RSRECR_FE = 1 << 0, // Framing error

	// Flag Register (depends on LCRH.FEN)
	FR_TXFE = 1 << 7, // Transmit FIFO empty
	FR_RXFF = 1 << 6, // Receive FIFO full
	FR_TXFF = 1 << 5, // Transmit FIFO full
	FR_RXFE = 1 << 4, // Receive FIFO empty
	FR_BUSY = 1 << 3, // BUSY transmitting data
	FR_CTS  = 1 << 0, // Clear To Send

	// Line Control Register
	LCRH_SPS   = 1 << 7, // sticky parity selected
	LCRH_WLEN  = 3 << 5, // word length (5, 6, 7 or 8 bit)
	LCRH_WLEN5 = 0 << 5, // word length 5 bit
	LCRH_WLEN6 = 1 << 5, // word length 6 bit
	LCRH_WLEN7 = 2 << 5, // word length 7 bit
	LCRH_WLEN8 = 3 << 5, // word length 8 bit
	LCRH_FEN   = 1 << 4, // Enable FIFOs
	LCRH_STP2  = 1 << 3, // Two stop bits select
	LCRH_EPS   = 1 << 2, // Even Parity Select
	LCRH_PEN   = 1 << 1, // Parity enable
	LCRH_BRK   = 1 << 0, // send break

	// Control Register
	CR_CTSEN  = 1 << 15, // CTS hardware flow control
	CR_RTSEN  = 1 << 14, // RTS hardware flow control
	CR_RTS    = 1 << 11, // (not) Request to send
	CR_RXE    = 1 <<  9, // Receive enable
	CR_TXW    = 1 <<  8, // Transmit enable
	CR_LBE    = 1 <<  7, // Loopback enable
	CR_UARTEN = 1 <<  0, // UART enable

	// Interrupts (IMSC / RIS / MIS / ICR)
	INT_ALL = 0x3F3,
    };

    constexpr volatile uint32_t * reg(uint32_t offset) {
	return Peripherals::reg(BASE + offset);
    }

    // initialize uart
    void init(void) {
        // Disable UART0.
	*reg(CR) = 0;

	// wait for end of transmission or reception(?)
	while(*reg(FR) & FR_BUSY) { }

	// flush transmit FIFO
	*reg(LCRH) &= ~LCRH_FEN;
	
	// Disable pull up/down for pin 14, 15
	GPIO::set_pull_up_down(14, GPIO::OFF);
	GPIO::set_pull_up_down(15, GPIO::OFF);

	// select function 0 for pin 14, 15
	GPIO::set_function(14, GPIO::FN0);
	GPIO::set_function(15, GPIO::FN0);

        // Clear pending interrupts.
	*reg(ICR) = INT_ALL;

        // Set integer & fractional part of baud rate.
        // Divider = UART_CLOCK/(16 * Baud)
        // Fraction part register = (Fractional part * 64) + 0.5
        // UART_CLOCK = 3000000; Baud = 115200.

        // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
        // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
	*reg(IBRD) = 1;
	*reg(FBRD) = 40;

        // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
	*reg(LCRH) = LCRH_FEN | LCRH_WLEN8;

        // Mask all interrupts.
	*reg(IMSC) = INT_ALL;

        // Enable UART0, receive & transfer part of UART.
	*reg(CR) = CR_UARTEN | CR_TXW | CR_RXE;
    }

    void put(uint8_t x) {
	// wait for space in the send buffer
	while(*reg(FR) & FR_TXFF) {	}
	*reg(DR) = x;
    }

    uint8_t get(void) {
	// wait for something in the receive buffer
	while(*reg(FR) & FR_RXFE) { }
	return *reg(DR);
    }

    bool poll(void) {
	// any input pending?
	return !(*reg(FR) & FR_RXFE);
    }
}

