/*
 * Copyright (C) 2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/printk.h>
#include <linux/spinlock.h>

#include <asm/time.h>
#include <asm/brcmstb/brcmstb.h>

/* MIPS clock measured at boot time.  Value is not changed by PM. */
unsigned long brcm_cpu_khz;

/* Sampling period for MIPS calibration.  50 = 1/50 of a second. */
#define SAMPLE_PERIOD		50

/***********************************************************************
 * UPG clocksource
 ***********************************************************************/

static cycle_t upg_cs_read(struct clocksource *cs)
{
	return BDEV_RD_F(TIMER_TIMER3_STAT, COUNTER_VAL);
}

static struct clocksource clocksource_upg = {
	.name		= "upg",
	.read		= upg_cs_read,
	.mask		= CLOCKSOURCE_MASK(30),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static inline void __init init_upg_clocksource(void)
{
	BDEV_WR_RB(BCHP_TIMER_TIMER3_CTRL, 0);
	BDEV_WR_F_RB(TIMER_TIMER_IS, TMR3TO, 1);
	BDEV_WR_RB(BCHP_TIMER_TIMER3_CTRL, 0xbfffffff);

	clocksource_upg.rating = 250;
	clocksource_register_hz(&clocksource_upg, UPGTMR_FREQ);
}

#ifdef CONFIG_BRCM_HAS_WKTMR

/***********************************************************************
 * WKTMR clocksource
 ***********************************************************************/

static DEFINE_SPINLOCK(wktmr_lock);

static cycle_t wktmr_cs_read(struct clocksource *cs)
{
	struct wktmr_time t;
	unsigned long flags;

	spin_lock_irqsave(&wktmr_lock, flags);
	wktmr_read(&t);
	spin_unlock_irqrestore(&wktmr_lock, flags);

	return (t.sec * (cycle_t)WKTMR_FREQ) + t.pre;
}

static struct clocksource clocksource_wktmr = {
	.name		= "wktmr",
	.read		= wktmr_cs_read,
	.mask		= CLOCKSOURCE_MASK(64),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static inline void __init init_wktmr_clocksource(void)
{
	clocksource_wktmr.rating = 250;
	clocksource_register_hz(&clocksource_wktmr, WKTMR_FREQ);
}

/***********************************************************************
 * WKTMR utility functions (boot time only)
 ***********************************************************************/

void wktmr_read(struct wktmr_time *t)
{
	uint32_t tmp;

	do {
		t->sec = BDEV_RD(BCHP_WKTMR_COUNTER);
		tmp = BDEV_RD(BCHP_WKTMR_PRESCALER_VAL);
	} while (tmp >= WKTMR_FREQ);

	t->pre = WKTMR_FREQ - tmp;
}

unsigned long wktmr_elapsed(struct wktmr_time *t)
{
	struct wktmr_time now;

	wktmr_read(&now);
	now.sec -= t->sec;
	if (now.pre > t->pre) {
		now.pre -= t->pre;
	} else {
		now.pre = WKTMR_FREQ + now.pre - t->pre;
		now.sec--;
	}
	return (now.sec * WKTMR_FREQ) + now.pre;
}

/*
 * MIPS frequency calibration (WKTMR)
 */

static __init unsigned long brcm_mips_freq(void)
{
	struct wktmr_time start;

	wktmr_read(&start);
	write_c0_count(0);

	while (wktmr_elapsed(&start) < (WKTMR_FREQ / SAMPLE_PERIOD))
		;

	return read_c0_count() * SAMPLE_PERIOD;
}

#else /* CONFIG_BRCM_HAS_WKTMR */

/*
 * MIPS frequency calibration (UPG TIMER3)
 */

static __init unsigned long brcm_mips_freq(void)
{
	unsigned long ret;

	/* reset countdown timer */
	BDEV_WR_RB(BCHP_TIMER_TIMER3_CTRL, 0);
	BDEV_WR_F_RB(TIMER_TIMER_IS, TMR3TO, 1);

	/* set up for countdown */
	BDEV_WR(BCHP_TIMER_TIMER0_CTRL, 0xc0000000 |
		(UPGTMR_FREQ / SAMPLE_PERIOD));
	write_c0_count(0);

	while ((BDEV_RD(BCHP_TIMER_TIMER_IS) & 1) == 0)
		;

	ret = read_c0_count();
	BDEV_WR(BCHP_TIMER_TIMER0_CTRL, 0);

	return ret * SAMPLE_PERIOD;
}

#endif /* CONFIG_BRCM_HAS_WKTMR */

/***********************************************************************
 * Timer setup
 ***********************************************************************/

void __init plat_time_init(void)
{
	unsigned int khz;

	pr_info("Measuring MIPS counter frequency...\n");
	mips_hpt_frequency = brcm_mips_freq();
	khz = mips_hpt_frequency / 1000;
#ifdef CONFIG_BMIPS5000
	brcm_cpu_khz = mips_hpt_frequency * 8 / 1000;
#else
	brcm_cpu_khz = mips_hpt_frequency * 2 / 1000;
#endif

	pr_info("Detected MIPS clock frequency: %lu MHz (%u.%03u MHz counter)\n",
		brcm_cpu_khz / 1000, khz / 1000, khz % 1000);

#ifdef CONFIG_CSRC_WKTMR
	init_wktmr_clocksource();
#endif
#ifdef CONFIG_CSRC_UPG
	init_upg_clocksource();
#endif
}

#if defined(CONFIG_PM_OPS) && defined(CONFIG_BRCM_HAS_WKTMR)
#include <linux/sysdev.h>

static struct wktmr_time suspend_start_time;

/*
 * save time of suspend start
 */
static int
timeclock_suspend(struct sys_device *dev, pm_message_t state)
{
	wktmr_read(&suspend_start_time);
	return 0;
}

/*
 *  update clock by elapsed time.
 */
static int
timeclock_resume(struct sys_device *dev)
{
	struct timespec ts;
	struct wktmr_time now, delta, *start;

	start = &suspend_start_time;

	wktmr_read(&now);
	delta.sec = now.sec - start->sec;
	if (now.pre > start->pre) {
		delta.pre = now.pre - start->pre;
	} else {
		delta.pre = WKTMR_FREQ + now.pre - start->pre;
		delta.sec--;
	}

	/*
	 * update clock
	 * Do not have to worry about the wake timer counter wrapping
	 */
	local_irq_enable();

	getnstimeofday(&ts);
	ts.tv_sec += delta.sec;
	ts.tv_nsec += delta.pre * (1000000000/WKTMR_FREQ);
	do_settimeofday(&ts);

	local_irq_disable();

	pr_debug("Time adjusted %d.%03d seconds, start %d.%03d end %d.%03d\n",
	    delta.sec, delta.pre / (WKTMR_FREQ/1000),
	    start->sec, start->pre / (WKTMR_FREQ/1000),
	    now.sec, now.pre / (WKTMR_FREQ/1000));

	return 0;
}

/* sysfs resume/suspend bits for timekeeping */
static struct sysdev_class timeclock_sysclass = {
	.name           = "timeclock",
	.resume         = timeclock_resume,
	.suspend        = timeclock_suspend,
};

static struct sys_device device_timeclock = {
	.id             = 0,
	.cls            = &timeclock_sysclass,
};

static int __init timeclock_init_device(void)
{
	int error = sysdev_class_register(&timeclock_sysclass);
	if (!error)
		error = sysdev_register(&device_timeclock);
	return error;
}

/*
 * Registration has to be later than the 'timekeeping' registration
 * in kernel/time/timekeeping.c
 */
late_initcall(timeclock_init_device);

#endif
