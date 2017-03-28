/*
 * Moppy as a kernel module :-)
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
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>


static struct hrtimer hr_timer;

#define PERIOD 				40000
#define FIRST_PIN      2
#define PIN_MAX				17
#define LOW						 0
#define HIGH					 1

#define MAX_DRIVE_POS		   158


static int currentPosition[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int currentState[] =
{
    0, 0, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW
};

static unsigned int currentPeriod[] =
{
    0, 0 , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned int currentTick[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

struct pinmap_t {
	int pin;
	char *label;
};

static struct pinmap_t pinMap[] =
{
	{ -1, "PIN0"},
	{ -1, "PIN1"},
	{  2, "PIN2"},     // track 1
	{  3, "PIN3"},
	{ 17, "PIN4"},     // track 2
	{ 18, "PIN5"},
	{ 27, "PIN6"},     // track 3
	{ 22, "PIN7"},
  { 23, "PIN8"},     // track 4
	{ 24, "PIN9"},
  { 25, "PIN10"},    // track 5
	{  4, "PIN11"},
	{ -1, "PIN12"},    // track 6
	{ -1, "PIN13"},
	{ -1, "PIN14"},    // track 7
	{ -1, "PIN15"},
	{ -1, "PIN16"},    // track 8
	{ -1, "PIN17"},
};


void digital_write(int pin, int value)
{
	int mapped_pin = pinMap[pin].pin;

	if(mapped_pin != -1) {
		gpio_set_value(mapped_pin, value);
	}
}

void toggle_pin(int pin)
{
	int direction_pin = pin + 1;

	if(pin < 2 || pin > 16) {
		return;
	}

    // Switch directions if end has been reached
    if (currentPosition[pin] >= MAX_DRIVE_POS)
    {
        currentState[direction_pin] = HIGH;
        digital_write(direction_pin, HIGH);
    }
    else if (currentPosition[pin] <= 0)
    {
        currentState[direction_pin] = LOW;
        digital_write(direction_pin, LOW);
    }

    // Update currentPosition
    if (currentState[direction_pin] == HIGH)
    {
        currentPosition[pin]--;
    }
    else
    {
        currentPosition[pin]++;
    }

    // Pulse the control pin
    digital_write(pin, currentState[pin]);
    currentState[pin] = ~currentState[pin];
}

// Resets all the pins
void reset(void)
{
	int p = 0;
	int s = 0;

    // Stop all notes (don't want to be playing during/after reset)
    for(p = FIRST_PIN; p <= PIN_MAX; p += 2) {
        currentPeriod[p] = 0;
    }

    // New all-at-once reset
    for(s = 0; s <= MAX_DRIVE_POS / 2; s++) {
        for(p = FIRST_PIN; p <= PIN_MAX; p += 2) {
            digital_write(p + 1, HIGH);
            digital_write(p, HIGH);
            digital_write(p, LOW);
        }
		mdelay(1);		// was 5
    }

    for(p = FIRST_PIN; p <= PIN_MAX; p += 2) {
        currentPosition[p] = 0;
        digital_write(p + 1, LOW);
        currentState[p + 1] = 0;
    }
}

static ssize_t sysfs_command_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int pin = 0;
	int value = 0;

	if(sscanf(buf, "%d, %d", &pin, &value) == 2) {

		printk("moppy: received command %d, %d\n", pin, value);

		if(pin >= FIRST_PIN && pin <= PIN_MAX) {
			currentPeriod[pin] = value;
		}
		else if(pin == 100 && value == 0) {
			reset();
		}
	}
	else {
		printk(KERN_ERR "moppy: received invalid coammnd\n");
	}

    return count;
}

static struct kobj_attribute command_attribute = __ATTR(command, 0220, NULL, sysfs_command_store);

/* SYSFS: List of all attributes exported to sysfs */
static struct attribute *attrs[] = {
    &command_attribute.attr,
    NULL,
};

/* SYSFS: Attributes for sysfs in a group */
static struct attribute_group attr_group = {
    .attrs = attrs,
};

/* SYSFS: Kernel object for sysfs */
static struct kobject *moppy_kobj;


#define HANDLE_TICK(pin) \
	if(currentPeriod[pin] > 0) \
    { \
        currentTick[pin]++; \
        if (currentTick[pin] >= currentPeriod[pin]) \
        { \
            toggle_pin(pin); \
            currentTick[pin] = 0; \
        } \
    }

/*
 * Timer function called periodically
 */
enum hrtimer_restart tick(struct hrtimer *timer_for_restart)
{
  	ktime_t currtime;
	ktime_t interval;

  	currtime  = ktime_get();
  	interval = ktime_set(0, PERIOD);

  	hrtimer_forward(timer_for_restart, currtime, interval);

	HANDLE_TICK(2);
	HANDLE_TICK(4);
	HANDLE_TICK(6);
	HANDLE_TICK(8);
	HANDLE_TICK(10);
	HANDLE_TICK(12);
	HANDLE_TICK(14);
	HANDLE_TICK(16);

	return HRTIMER_RESTART;
}


/*
 * Module init function
 */
static int __init moppy_init(void)
{
	int 	ret 		= 0;
	int		p   		= 0;
	ktime_t interval;

	printk(KERN_INFO "%s\n", __func__);

	/* register sysfs entry */
    moppy_kobj = kobject_create_and_add("moppy", kernel_kobj);

    if(!moppy_kobj)
    {
        return -ENOMEM;
    }

    ret = sysfs_create_group(moppy_kobj, &attr_group);

    if(ret)
    {
        kobject_put(moppy_kobj);
        return ret;
    }

    printk(KERN_INFO "moppy: registered command interface: /sys/kernel/moppy/command\n");

	for(p = FIRST_PIN; p <= PIN_MAX; p++) {
		if(pinMap[p].pin != -1) {
			ret = gpio_request_one(pinMap[p].pin, GPIOF_OUT_INIT_LOW, pinMap[p].label);

			if (ret) {
				printk(KERN_ERR "moppy: unable to rgister GPIO #%d (%s) -> DISABLED\n", pinMap[p].pin, pinMap[p].label);
				pinMap[p].pin = -1;
			}
      else {
        printk(KERN_INFO "moppy: registered GPIO #%d (%s)\n", pinMap[p].pin, pinMap[p].label);
      }
		}
  }

	reset();

	/* init timer, add timer function */
	interval = ktime_set(0, PERIOD);
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &tick;
	hrtimer_start(&hr_timer, interval, HRTIMER_MODE_REL);

	return ret;
}

/*
 * Module exit function
 */
static void __exit moppy_exit(void)
{
	int p 		= 0;
	int ret 	= 0;

	printk(KERN_INFO "%s\n", __func__);

	/* remove kobj */
  kobject_put(moppy_kobj);
	hrtimer_cancel(&hr_timer);

	for(p = FIRST_PIN; p <= PIN_MAX; p++) {
		if(pinMap[p].pin != -1) {
			gpio_set_value(pinMap[p].pin, LOW);
			gpio_free(pinMap[p].pin);
		}
  }
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefan Wendler");
MODULE_DESCRIPTION("Moppy kernel module");

module_init(moppy_init);
module_exit(moppy_exit);
