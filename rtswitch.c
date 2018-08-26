/*
 *
 * rtswitch.c
 * device driver of Raspberry Pi Gibbon
 *
 * Copyright (C) 2017 Daisuke Sato <tiryoh@gmail.com>
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
static struct class *class_switch = NULL;
static volatile uint32_t *gpio_base = NULL;

static volatile int cdev_index = 0;
static volatile int open_counter = 0;

#define MAX_BUFLEN 64
unsigned char sw_buf[MAX_BUFLEN];
static int buflen = 0;

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

#define NUM_DEV_SWITCH 3
#define NUM_DEV_TOTAL NUM_DEV_SWITCH

#define SW1_PIN 21
#define SW2_PIN 26
#define SW3_PIN 20

#define DEVNAME_SWITCH "rtswitch"

static int _major_switch = DEV_MAJOR;
static int _minor_switch = DEV_MINOR;

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

static ssize_t sw_read(struct file *filep, char __user *buf, size_t count,
                       loff_t *pos) {
  unsigned int ret = 0;
  unsigned int pin = SW1_PIN;
  int index;
  uint32_t mask;
  int minor = *((int *)filep->private_data);

  switch (minor) {
  case 0:
    pin = SW1_PIN;
    break;
  case 1:
    pin = SW2_PIN;
    break;
  case 2:
    pin = SW3_PIN;
    break;
  default:
    return 0;
    break;
  }

  if (*pos > 0) return 0; // End of file

  gpio_base[37] = GPIO_PULLUP & 0x03; //  GPPUD
  msleep(1);
  gpio_base[38] = 0x01 << pin; //  GPPUDCLK0
  msleep(1);
  gpio_base[37] = 0;
  gpio_base[38] = 0;

  index = RPI_GPFSEL0_INDEX + pin / 10;
  mask = ~(0x07 << ((pin % 10) * 3));

  ret = ((gpio_base[13] & (0x01 << pin)) != 0);
  sprintf(sw_buf, "%d\n", ret);

  buflen = strlen(sw_buf);
  count = buflen;

  if (copy_to_user((void *)buf, &sw_buf, count)) {
    printk(KERN_INFO "err read buffer from ret  %d\n", ret);
    printk(KERN_INFO "err read buffer from %s\n", sw_buf);
    printk(KERN_INFO "err sample_char_read size(%d)\n", count);
    printk(KERN_INFO "sample_char_read size err(%d)\n", -EFAULT);
    return 0;
  }
  *pos += count;
  buflen = 0;
  return count;
}

static int switch_gpio_map(void) {
  if (gpio_base == NULL) {
    gpio_base = ioremap_nocache(RPI_GPIO_BASE, RPI_GPIO_SIZE);
  }
  rpi_gpio_function_set(SW1_PIN, RPI_GPF_INPUT);
  rpi_gpio_function_set(SW2_PIN, RPI_GPF_INPUT);
  rpi_gpio_function_set(SW3_PIN, RPI_GPF_INPUT);
  return 0;
}

static int dev_open(struct inode *inode, struct file *filep) {
  int retval;
  int *minor = (int *)kmalloc(sizeof(int), GFP_KERNEL);
  int major = MAJOR(inode->i_rdev);
  *minor = MINOR(inode->i_rdev);

  printk(KERN_INFO "open request major:%d minor: %d \n", major, *minor);

  filep->private_data = (void *)minor;

  retval = switch_gpio_map();
  if (retval != 0) {
      printk(KERN_ERR "Can not open switch gpio.\n");
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

static struct file_operations sw_fops = {
    .open = dev_open, .read = sw_read, .release = dev_release,
};

static int switch_register_dev(void) {
  int retval;
  dev_t dev;
  int i;
  dev_t devno;

  retval = alloc_chrdev_region(&dev, DEV_MINOR, NUM_DEV_SWITCH, DEVNAME_SWITCH);
  if (retval < 0) {
    printk(KERN_ERR "alloc_chrdev_region failed.\n");
    return retval;
  }
  _major_switch = MAJOR(dev);

  class_switch = class_create(THIS_MODULE, DEVNAME_SWITCH);
  if (IS_ERR(class_switch)) {
    return PTR_ERR(class_switch);
  }

  for (i = 0; i < NUM_DEV_SWITCH; i++) {
    devno = MKDEV(_major_switch, _minor_switch + i);
    cdev_init(&(cdev_array[cdev_index]), &sw_fops);
    cdev_array[cdev_index].owner = THIS_MODULE;

    if (cdev_add(&(cdev_array[cdev_index]), devno, 1) < 0) {
      printk(KERN_ERR "cdev_add failed minor = %d\n", _minor_switch + i);
    } else {
      device_create(class_switch, NULL, devno, NULL, DEVNAME_SWITCH "%u",
                    _minor_switch + i);
    }
    cdev_index++;
  }
  return 0;
}

static int __init init_mod(void) {
  int retval;
  size_t size;

  printk(KERN_INFO "%d\n", NUM_DEV_TOTAL);
  printk(KERN_INFO "%s loading...\n", DEVNAME_SWITCH);

  retval = switch_gpio_map();
  if (retval != 0) {
    printk(KERN_ALERT "Can not use GPIO registers.\n");
    return -EBUSY;
  }

  size = sizeof(struct cdev) * NUM_DEV_TOTAL;
  cdev_array = (struct cdev *)kmalloc(size, GFP_KERNEL);

  retval = switch_register_dev();
  if (retval != 0) {
    printk(KERN_ALERT "switch driver register failed.\n");
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

  devno_top = MKDEV(_major_switch, _minor_switch);
  for (i = 0; i < NUM_DEV_SWITCH; i++) {
    devno = MKDEV(_major_switch, _minor_switch + i);
    device_destroy(class_switch, devno);
  }
  unregister_chrdev_region(devno_top, NUM_DEV_SWITCH);

  class_destroy(class_switch);

  kfree(cdev_array);
  printk("module being removed at %lu\n", jiffies);
}

module_init(init_mod);
module_exit(cleanup_mod);
