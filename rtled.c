/*
 *
 * rtled.c
 * device driver of Raspberry Pi Gibbon
 *
 * Copyright (C) 2017 Tiryoh <tiryoh@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("Daisuke Sato");
MODULE_DESCRIPTION("device driver of Raspberry Pi Gibbon");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

static struct cdev *cdev_array = NULL;
static struct class *class_led = NULL;
static volatile uint32_t *gpio_base = NULL;

static volatile int cdev_index = 0;
static volatile int open_counter = 0;

#define DEV_MAJOR 0
#define DEV_MINOR 0

#define RPI_GPIO_P2MASK (uint32_t)0xffffffff

#define RPI_REG_BASE 0x3f000000

#define RPI_GPIO_OFFSET 0x200000
#define RPI_GPIO_SIZE 0xC0
#define RPI_GPIO_BASE (RPI_REG_BASE + RPI_GPIO_OFFSET)
#define REG_GPIO_NAME "RaspberryPi GPIO"

#define RPI_GPF_INPUT 0x00
#define RPI_GPF_OUTPUT 0x01

#define RPI_GPFSEL0_INDEX 0
#define RPI_GPFSEL1_INDEX 1
#define RPI_GPFSEL2_INDEX 2
#define RPI_GPFSEL3_INDEX 3

#define RPI_GPSET0_INDEX 7
#define RPI_GPCLR0_INDEX 10

#define GPIO_PULLNONE 0x0
#define GPIO_PULLDOWN 0x1
#define GPIO_PULLUP 0x2

#define NUM_DEV_LED 4
#define NUM_DEV_TOTAL NUM_DEV_LED

#define LED0_BASE 18
#define LED1_BASE 23
#define LED2_BASE 24
#define LED3_BASE 25

#define DEVNAME_LED "rtled"

static int _major_led = DEV_MAJOR;
static int _minor_led = DEV_MINOR;

static int rpi_gpio_function_set(int pin, uint32_t func) {
  int index = RPI_GPFSEL0_INDEX + pin / 10;
  uint32_t shift = (pin % 10) * 3;
  uint32_t mask = ~(0x07 << shift);
  gpio_base[index] = (gpio_base[index] & mask) | ((func & 0x07) << shift);
  return 1;
}

static void rpi_gpio_set32(uint32_t mask, uint32_t val) {
  gpio_base[RPI_GPSET0_INDEX] = val & mask;
}

static void rpi_gpio_clear32(uint32_t mask, uint32_t val) {
  gpio_base[RPI_GPCLR0_INDEX] = val & mask;
}

static int led_put(int ledno) {
  switch (ledno) {
  case 0:
    rpi_gpio_set32(RPI_GPIO_P2MASK, 1 << LED0_BASE);
    break;
  case 1:
    rpi_gpio_set32(RPI_GPIO_P2MASK, 1 << LED1_BASE);
    break;
  case 2:
    rpi_gpio_set32(RPI_GPIO_P2MASK, 1 << LED2_BASE);
    break;
  case 3:
    rpi_gpio_set32(RPI_GPIO_P2MASK, 1 << LED3_BASE);
    break;
  }
  return 0;
}

static int led_del(int ledno) {
  switch (ledno) {
  case 0:
    rpi_gpio_clear32(RPI_GPIO_P2MASK, 1 << LED0_BASE);
    break;
  case 1:
    rpi_gpio_clear32(RPI_GPIO_P2MASK, 1 << LED1_BASE);
    break;
  case 2:
    rpi_gpio_clear32(RPI_GPIO_P2MASK, 1 << LED2_BASE);
    break;
  case 3:
    rpi_gpio_clear32(RPI_GPIO_P2MASK, 1 << LED3_BASE);
    break;
  }
  return 0;
}

static ssize_t led_write(struct file *filep, const char __user *buf, size_t count,
                         loff_t *pos) {
  char cval;
  int ret;
  int minor = *((int *)filep->private_data);

  if (count > 0) {
    if (copy_from_user(&cval, buf, sizeof(char))) {
      return -EFAULT;
    }
    switch (cval) {
    case '1':
      ret = led_put(minor);
      break;
    case '0':
      ret = led_del(minor);
      break;
    }
    return sizeof(char);
  }
  return 0;
}

static int led_gpio_map(void) {
  if (gpio_base == NULL) {
    gpio_base = ioremap_nocache(RPI_GPIO_BASE, RPI_GPIO_SIZE);
  }
  rpi_gpio_function_set(LED0_BASE, RPI_GPF_OUTPUT);
  rpi_gpio_function_set(LED1_BASE, RPI_GPF_OUTPUT);
  rpi_gpio_function_set(LED2_BASE, RPI_GPF_OUTPUT);
  rpi_gpio_function_set(LED3_BASE, RPI_GPF_OUTPUT);
  return 0;
}

static int dev_open(struct inode *inode, struct file *filep) {
  int retval;
  int *minor = (int *)kmalloc(sizeof(int), GFP_KERNEL);
  int major = MAJOR(inode->i_rdev);
  *minor = MINOR(inode->i_rdev);

  printk(KERN_INFO "open request major:%d minor: %d \n", major, *minor);

  filep->private_data = (void *)minor;

  retval = led_gpio_map();
  if (retval != 0) {
    printk(KERN_ERR "Can not open led.\n");
    return retval;
  }
  open_counter++;
  return 0;
}

static int dev_release(struct inode *inode, struct file *filep) {
  kfree(filep->private_data);
  open_counter--;
  if (open_counter <= 0) {
    iounmap(gpio_base);
    gpio_base = NULL;
  }
  return 0;
}

static struct file_operations led_fops = {
    .open = dev_open, .release = dev_release, .write = led_write,
};

static int led_register_dev(void) {
  int retval;
  dev_t dev;
  dev_t devno;
  int i;

  retval = alloc_chrdev_region(&dev, DEV_MINOR, NUM_DEV_LED, DEVNAME_LED);

  if (retval < 0) {
    printk(KERN_ERR "alloc_chrdev_region failed.\n");
    return retval;
  }
  _major_led = MAJOR(dev);

  class_led = class_create(THIS_MODULE, DEVNAME_LED);
  if (IS_ERR(class_led)) {
    return PTR_ERR(class_led);
  }

  for (i = 0; i < NUM_DEV_LED; i++) {
    devno = MKDEV(_major_led, _minor_led + i);

    cdev_init(&(cdev_array[cdev_index]), &led_fops);
    cdev_array[cdev_index].owner = THIS_MODULE;
    if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
      printk(KERN_ERR "cdev_add failed minor = %d\n", _minor_led + i);
    } else {
      device_create(class_led, NULL, devno, NULL, DEVNAME_LED "%u",
                    _minor_led + i);
    }
    cdev_index++;
  }
  return 0;
}

static int __init init_mod(void) {
  int retval;
  size_t size;

  printk(KERN_INFO "%d\n", NUM_DEV_TOTAL);
  printk(KERN_INFO "%s loading...\n", DEVNAME_LED);

  retval = led_gpio_map();
  if (retval != 0) {
    printk(KERN_ALERT "Can not use GPIO registers.\n");
    return -EBUSY;
  }

  size = sizeof(struct cdev) * NUM_DEV_TOTAL;
  cdev_array = (struct cdev *)kmalloc(size, GFP_KERNEL);

  retval = led_register_dev();
  if (retval != 0) {
    printk(KERN_ALERT "rtleddriver register failed.\n");
    return retval;
  }
  return 0;
}

static void __exit cleanup_mod(void) {
  int i;
  dev_t devno;
  dev_t devno_top;

  for (i = 0; i < NUM_DEV_TOTAL; i++) {
    cdev_del(&(cdev_array[i]));
  }

  devno_top = MKDEV(_major_led, _minor_led);
  for (i = 0; i < NUM_DEV_LED; i++) {
    devno = MKDEV(_major_led, _minor_led + i);
    device_destroy(class_led, devno);
  }
  unregister_chrdev_region(devno_top, NUM_DEV_LED);

  class_destroy(class_led);

  kfree(cdev_array);
  printk("module being removed at %lu\n", jiffies);
}

module_init(init_mod);
module_exit(cleanup_mod);
