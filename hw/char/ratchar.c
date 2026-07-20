#include "qemu/osdep.h"

#include "hw/char/ratchar.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/core/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/log.h"

DeviceState *ratchar_create(hwaddr addr, qemu_irq irq, Chardev *chr) {
  DeviceState *dev;
  SysBusDevice *s;

  g_printf("rat: create\n");

  dev = qdev_new("ratchar");
  s = SYS_BUS_DEVICE(dev);
  qdev_prop_set_chr(dev, "chardev", chr);
  sysbus_realize_and_unref(s, &error_fatal);
  sysbus_mmio_map(s, 0, addr);
  sysbus_connect_irq(s, 0, irq);

  return dev;
}

static const Property ratchar_properties[] = {
    DEFINE_PROP_CHR("chardev", RatcharState, chr),
};

static void ratchar_fifo_init(struct RatcharFIFO *fifo) {
  fifo->count = 0;
  fifo->head = 0;
  fifo->tail = 0;
}

static bool ratchar_fifo_full(struct RatcharFIFO *fifo) {
  return fifo->count == RATCHAR_FIFO_DEPTH;
}

static bool ratchar_fifo_empty(struct RatcharFIFO *fifo) {
  return fifo->count == 0;
}

static void ratchar_fifo_push(struct RatcharFIFO *r, uint8_t c) {
  if (ratchar_fifo_full(r)) {
    // TODO
    return;
  }

  r->buf[r->tail++] = c;
  r->tail &= 0b111;
  r->count++;
}

static uint8_t ratchar_fifo_pop(struct RatcharFIFO *r) {
  uint8_t res;

  if (ratchar_fifo_empty(r)) {
    // TODO
    res = 0;
  } else {
    res = r->buf[r->head];
    r->head &= 0b111;
    r->count--;
  }

  return res;
}

static void ratchar_update(RatcharState *r) {
  bool rx_avail;
  bool rx_irq_pending, rx_error_irq_pending;

  rx_avail = !ratchar_fifo_empty(&r->rx);

  if (r->ctrl.bits.rx_clear_error) {
    r->status.bits.rx_overrun = 0;
    r->ctrl.bits.rx_clear_error = 0;
  }

  if (r->ctrl.bits.rx_clear_irq) {
    r->status.bits.rx_irq_pending = 0;
    r->ctrl.bits.rx_clear_irq = 0;
  }

  if (r->ctrl.bits.rx_clear_error_irq) {
    r->status.bits.rx_overrun_irq_pending = 0;
    r->ctrl.bits.rx_clear_error_irq = 0;
  }

  rx_irq_pending = r->status.bits.rx_irq_pending;
  rx_error_irq_pending = r->status.bits.rx_overrun_irq_pending;

  r->status.bits.rx_avail = rx_avail;
  r->status.bits.rx_irq_pending =
      rx_irq_pending || (rx_avail && r->ctrl.bits.rx_irq_on);

  r->status.bits.rx_overrun_irq_pending =
      rx_error_irq_pending ||
      (r->status.bits.rx_overrun && r->ctrl.bits.rx_overrun_irq_on);

  qemu_set_irq(r->irq[RATCHAR_IRQ_RX], r->status.bits.rx_irq_pending);
  qemu_set_irq(r->irq[RATCHAR_IRQ_ERROR],
               r->status.bits.rx_overrun_irq_pending);
  qemu_set_irq(r->irq[RATCHAR_IRQ_COMBO],
               r->status.bits.rx_overrun_irq_pending |
                   r->status.bits.rx_irq_pending);
}

static void ratchar_fifo_rx_put(RatcharState *r, uint8_t c) {
  if (!r->ctrl.bits.rx_on)
    return;

  if (ratchar_fifo_full(&r->rx)) {
    r->status.bits.rx_overrun = true;
    return;
  }

  ratchar_fifo_push(&r->rx, c);
}

static uint8_t ratchar_rx_receive(RatcharState *r) {
  uint8_t c;

  c = ratchar_fifo_pop(&r->rx);
  ratchar_update(r);
  qemu_chr_fe_accept_input(&r->chr);
  return c;
}

static void ratchar_transmit(RatcharState *r, uint8_t c) {
  // g_printf("TX: %c\n", c);g_printf("TX: %c\n", c);
  if (r->ctrl.bits.tx_on)
    qemu_chr_fe_write_all(&r->chr, &c, 1);
}

static uint64_t ratchar_read(void *obj, hwaddr addr, uint32_t size) {
  RatcharState *rs;
  hwaddr offset;
  uint64_t r;

  rs = RATCHAR(obj);
  offset = addr >> 2;
  switch (offset) {
  case 0:
    r = ratchar_rx_receive(rs);
    break;
  case 1:
    r = rs->status.u32;
    break;
  case 2:
    r = rs->ctrl.u32;
    break;
  default:
    qemu_log_mask(LOG_GUEST_ERROR, "ratchar_write: Bad offset 0x%x\n",
                  (int)offset);
    r = 0;
  }

  return r;
}

static void ratchar_write(void *obj, hwaddr addr, uint64_t data,
                          uint32_t size) {
  RatcharState *rs = RATCHAR(obj);
  hwaddr offset;

  offset = addr >> 2;
  switch (offset) {
  case 0:
    ratchar_transmit(rs, (uint8_t)data);
    break;
  case 1:
    rs->status.u32 = (uint32_t)data;
    break;
  case 2:
    rs->ctrl.u32 = (uint32_t)data;
    ratchar_update(rs);
    break;
  default:
    qemu_log_mask(LOG_GUEST_ERROR, "ratchar_write: Bad offset 0x%x\n",
                  (int)offset);
  }
}

static const MemoryRegionOps rat_ops = {
    .read = ratchar_read,
    .write = ratchar_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

static void ratchar_init(Object *obj) {
  SysBusDevice *ds = SYS_BUS_DEVICE(obj);
  RatcharState *rs = RATCHAR(obj);
  unsigned int i;

  memory_region_init_io(&rs->iomem, OBJECT(rs), &rat_ops, rs, "ratchar", 4096);
  sysbus_init_mmio(ds, &rs->iomem);

  for (i = 0; i < ARRAY_SIZE(rs->irq); i++) {
    sysbus_init_irq(ds, &rs->irq[i]);
  }

  ratchar_fifo_init(&rs->rx);

  g_printf("rat: init\n");
}

static int ratchar_can_receive(void *obj) {
  RatcharState *r = RATCHAR(obj);

  return r->ctrl.bits.rx_on;
}

static void ratchar_receive(void *obj, const uint8_t *data, int n) {
  int i;
  RatcharState *r;

  r = RATCHAR(obj);
  for (i = 0; i < n; i++)
    ratchar_fifo_rx_put(RATCHAR(obj), data[i]);

  ratchar_update(r);
}

static void ratchar_event(void *obj, QEMUChrEvent ev) {
  RatcharState *r;

  r = RATCHAR(obj);
  if (ev == CHR_EVENT_BREAK) {
    ratchar_fifo_rx_put(r, 0);
    ratchar_update(r);
  }
}

static void ratchar_realize(struct DeviceState *dev, Error **e) {
  (void)dev;
  (void)e;

  RatcharState *s = RATCHAR(dev);
  qemu_chr_fe_set_handlers(&s->chr, ratchar_can_receive, ratchar_receive,
                           ratchar_event, NULL, s, NULL, true);
  g_printf("rat: realize\n");
}

static void ratchar_unrealize(struct DeviceState *dev) {
  // RatcharState *r = RATCHAR(dev);
  g_printf("rat: unrealize\n");
}

static void ratchar_class_init(ObjectClass *klass, const void *data) {
  (void)data;
  DeviceClass *dc = DEVICE_CLASS(klass);

  g_printf("rat: class_init\n");

  device_class_set_props(dc, ratchar_properties);
  dc->realize = ratchar_realize;
  dc->unrealize = ratchar_unrealize;
}

static const TypeInfo ratchar_info = {.name = TYPE_RATCHAR,
                                      .parent = TYPE_SYS_BUS_DEVICE,
                                      .instance_size = sizeof(RatcharState),
                                      .instance_init = ratchar_init,
                                      .class_init = ratchar_class_init};

static void pl011_register_types(void) { type_register_static(&ratchar_info); }

type_init(pl011_register_types)
