#include "qemu/osdep.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/core/qdev-properties.h"
#include "qapi/error.h"
#include "hw/char/ratchar.h"

DeviceState *ratchar_create(hwaddr addr, qemu_irq irq, Chardev *chr) {
    DeviceState *dev;
    SysBusDevice *s;


    dev = qdev_new("ratchar");
    s = SYS_BUS_DEVICE(dev);
    qdev_prop_set_chr(dev, "chardev", chr);
    sysbus_realize_and_unref(s, &error_fatal);
    g_printf("+++++++++ %d\n", s->num_mmio);
    sysbus_mmio_map(s, 0, addr);
    sysbus_connect_irq(s, 0, irq);

    return dev;
}

static const Property ratchar_properties[] = {
    DEFINE_PROP_CHR("chardev", RatcharState, chr),
};

static void ratchar_init(Object *klass) {
    (void)klass;
}

static void ratchar_class_init(ObjectClass *klass, const void *data) {
    (void)data;
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, ratchar_properties);
}

static const TypeInfo ratchar_info = {
    .name          = TYPE_RATCHAR,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(RatcharState),
    .instance_init = ratchar_init,
    .class_init    = ratchar_class_init,
};

static void pl011_register_types(void)
{
    type_register_static(&ratchar_info);
}

type_init(pl011_register_types)
