/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_RATCHAR_H
#define HW_RATCHAR_H

#include "chardev/char-fe.h"
#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_RATCHAR "ratchar"
OBJECT_DECLARE_SIMPLE_TYPE(RatcharState, RATCHAR)

#define RATCHAR_FIFO_DEPTH 8

struct RatcharFIFO {
  uint8_t buf[RATCHAR_FIFO_DEPTH];
  uint8_t head;
  uint8_t tail;
  uint8_t count;
};

enum { RATCHAR_IRQ_RX, RATCHAR_IRQ_ERROR,RATCHAR_IRQ_COMBO, RATCHAR_IRQ_COUNT };

struct RatcharState {
  SysBusDevice parent_obj;
  MemoryRegion iomem;

  CharFrontend chr;
  qemu_irq irq[RATCHAR_IRQ_COUNT];
  struct RatcharFIFO rx;

  union {
    __attribute__((packed)) struct {
      uint32_t rx_avail : 1;
      uint32_t rx_irq_pending : 1;
      uint32_t rx_overrun : 1;
      uint32_t rx_overrun_irq_pending: 1;
      uint32_t combined_irq_pendig: 1;
    } bits;
    uint32_t u32;
  } status;

  union {
    __attribute__((packed)) struct {
      uint32_t tx_on : 1;
      uint32_t rx_on : 1;
      uint32_t rx_irq_on : 1;
      uint32_t rx_overrun_irq_on : 1;
      uint32_t rx_clear_irq : 1;
      uint32_t rx_clear_error : 1;
      uint32_t rx_clear_error_irq : 1;
    } bits;
    uint32_t u32;
  } ctrl;
};

DeviceState *ratchar_create(hwaddr addr, qemu_irq irq, Chardev *chr);

#endif
