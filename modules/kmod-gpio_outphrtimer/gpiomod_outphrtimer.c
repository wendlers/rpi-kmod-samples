/*
 * Basic kernel module using a timer and GPIOs to flash a LED.
 *
 * Author:
 * 	Stefan Wendler (devnull@kaltpost.de)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/gpio.h>

#include <linux/hrtimer.h>

#define PIN		4

const  unsigned long timer_interval_ns = 40000;			// 25kHz = 40.000ns

static struct hrtimer hr_timer;

static int pin_value = 0;

/*
 * Timer function called periodically
 */
enum hrtimer_restart timer_callback(struct hrtimer *timer_for_restart)
{
  	ktime_t currtime;
	ktime_t interval;

  	currtime  = ktime_get();
  	interval = ktime_set(0, timer_interval_ns);

  	hrtimer_forward(timer_for_restart, currtime, interval);

	gpio_set_value(PIN, pin_value);
	pin_value = !pin_value;

	return HRTIMER_RESTART;
}

/*
 * Module init function
 */
static int __init gpiomod_init(void)
{
	int 	ret = 0;
	ktime_t interval;

	printk(KERN_INFO "%s\n", __func__);

	// register, turn off
	ret = gpio_request_one(PIN, GPIOF_OUT_INIT_LOW, "PIN");

	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);
		return ret;
	}

	/* init timer, add timer function */
	interval = ktime_set(0, timer_interval_ns);
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &timer_callback;
	hrtimer_start(&hr_timer, interval, HRTIMER_MODE_REL);

	return ret;
}

/*
 * Module exit function
 */
static void __exit gpiomod_exit(void)
{
	int ret = 0;

	printk(KERN_INFO "%s\n", __func__);

	ret = hrtimer_cancel(&hr_timer);
	if(ret) {
		printk("Failed to cancel tiemr.\n");
	}

	// turn LED off
	gpio_set_value(PIN, 0);

	// unregister GPIO
	gpio_free(PIN);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefan Wendler");
MODULE_DESCRIPTION("Basic kernel module using high resolution timer.");

module_init(gpiomod_init);
module_exit(gpiomod_exit);
