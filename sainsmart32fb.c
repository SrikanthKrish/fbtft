/*
 * FB driver for the Sainsmart 3.2" LCD display
 *
 * Copyright (C) 2013 Noralf Tronnes
 *
 * Init sequence taken from ITDB02_Graph16.cpp - Copyright (C)2010-2011 Henning Karlsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "fbtft.h"

#define DRVNAME     "sainsmart32fb"
#define WIDTH       240
#define HEIGHT      320

static bool latched = false;
module_param(latched, bool, 0);
MODULE_PARM_DESC(latched, "Use with latched 16-bit databus");

static unsigned speed = 0;
module_param(speed, uint, 0);
MODULE_PARM_DESC(speed, "Parallel speed. 1=slow down");


static int sainsmart32fb_init_display(struct fbtft_par *par)
{
	fbtft_par_dbg(DEBUG_INIT_DISPLAY, par, "%s()\n", __func__);

	par->fbtftops.reset(par);

	write_reg(par, 0x00,0x0001);
	write_reg(par, 0x03,0xA8A4);
	write_reg(par, 0x0C,0x0000);
	write_reg(par, 0x0D,0x080C);
	write_reg(par, 0x0E,0x2B00);
	write_reg(par, 0x1E,0x00B7);
	write_reg(par, 0x01,0x2B3F);
	write_reg(par, 0x02,0x0600);
	write_reg(par, 0x10,0x0000);
	switch (par->info->var.rotate) {
	case 0:
		write_reg(par, 0x11, 0x6070);
		break;
	case 2:
		write_reg(par, 0x11, 0x6040);
		break;
	case 1:
		write_reg(par, 0x11, 0x6068);
		break;
	case 3:
		write_reg(par, 0x11, 0x6058);
		break;
	}
	write_reg(par, 0x05,0x0000);
	write_reg(par, 0x06,0x0000);
	write_reg(par, 0x16,0xEF1C);
	write_reg(par, 0x17,0x0003);
	write_reg(par, 0x07,0x0233);
	write_reg(par, 0x0B,0x0000);
	write_reg(par, 0x0F,0x0000);
	write_reg(par, 0x41,0x0000);
	write_reg(par, 0x42,0x0000);
	write_reg(par, 0x48,0x0000);
	write_reg(par, 0x49,0x013F);
	write_reg(par, 0x4A,0x0000);
	write_reg(par, 0x4B,0x0000);
	write_reg(par, 0x44,0xEF00);
	write_reg(par, 0x45,0x0000);
	write_reg(par, 0x46,0x013F);
	write_reg(par, 0x30,0x0707);
	write_reg(par, 0x31,0x0204);
	write_reg(par, 0x32,0x0204);
	write_reg(par, 0x33,0x0502);
	write_reg(par, 0x34,0x0507);
	write_reg(par, 0x35,0x0204);
	write_reg(par, 0x36,0x0204);
	write_reg(par, 0x37,0x0502);
	write_reg(par, 0x3A,0x0302);
	write_reg(par, 0x3B,0x0302);
	write_reg(par, 0x23,0x0000);
	write_reg(par, 0x24,0x0000);
	write_reg(par, 0x25,0x8000);
	write_reg(par, 0x4f,0x0000);
	write_reg(par, 0x4e,0x0000);
	write_reg(par, 0x22);   
	return 0;
}

static void sainsmart32fb_set_addr_win(struct fbtft_par *par, int xs, int ys, int xe, int ye)
{
	fbtft_par_dbg(DEBUG_SET_ADDR_WIN, par, "%s(xs=%d, ys=%d, xe=%d, ye=%d)\n", __func__, xs, ys, xe, ye);

	switch (par->info->var.rotate) {
	/* R4Eh - Set GDDRAM X address counter */
	/* R4Fh - Set GDDRAM Y address counter */
	case 0:
		write_reg(par, 0x4e, xs);
		write_reg(par, 0x4f, ys);
		break;
	case 2:
		write_reg(par, 0x4e, par->info->var.xres - 1 - xs);
		write_reg(par, 0x4f, par->info->var.yres - 1 - ys);
		break;
	case 1:
		write_reg(par, 0x4e, par->info->var.yres - 1 - ys);
		write_reg(par, 0x4f, xs);
		break;
	case 3:
		write_reg(par, 0x4e, ys);
		write_reg(par, 0x4f, par->info->var.xres - 1 - xs);
		break;
	}

	/* R22h - RAM data write */
	write_reg(par, 0x22);
}

int write_gpio16_wr_slow(struct fbtft_par *par, void *buf, size_t len)
{
	u16 data;
	int i;
#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
	static u16 prev_data = 0;
#endif

	fbtft_par_dbg(DEBUG_WRITE, par, "%s(len=%d)\n", __func__, len);

	while (len) {
		data = *(u16 *) buf;

		/* Start writing by pulling down /WR */
		gpio_set_value(par->gpio.wr, 0);

		/* Set data */
#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
		if (data == prev_data) {
			gpio_set_value(par->gpio.wr, 0); /* used as delay */
		} else {
			for (i=0;i<16;i++) {
				if ((data & 1) != (prev_data & 1))
					gpio_set_value(par->gpio.db[i], (data & 1));
				data >>= 1;
				prev_data >>= 1;
			}
		}
#else
		for (i=0;i<16;i++) {
			gpio_set_value(par->gpio.db[i], (data & 1));
			data >>= 1;
		}
#endif

		/* Pullup /WR */
		gpio_set_value(par->gpio.wr, 1);

#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
		prev_data = *(u16 *) buf;
#endif
		buf += 2;
		len -= 2;
	}

	return 0;
}

static int sainsmart32fb_verify_gpios(struct fbtft_par *par)
{
	int i;
	int num_db = 16;

	fbtft_par_dbg(DEBUG_VERIFY_GPIOS, par, "%s()\n", __func__);

	if (par->gpio.dc < 0) {
		dev_err(par->info->device, "Missing info about 'dc' gpio. Aborting.\n");
		return -EINVAL;
	}
	if (par->pdev) {
		if (par->gpio.wr < 0) {
			dev_err(par->info->device, "Missing info about 'wr' gpio. Aborting.\n");
			return -EINVAL;
		}
		if (latched && (par->gpio.latch < 0)) {
			dev_err(par->info->device, "Missing info about 'latch' gpio. Aborting.\n");
			return -EINVAL;
		}
		if (latched)
			num_db = 8;
		for (i=0;i < num_db;i++) {
			if (par->gpio.db[i] < 0) {
				dev_err(par->info->device, "Missing info about 'db%02d' gpio. Aborting.\n", i);
				return -EINVAL;
			}
		}
	}

	return 0;
}

struct fbtft_display sainsmart32fb_display = {
	.width = WIDTH,
	.height = HEIGHT,
};

static int sainsmart32fb_probe_common(struct spi_device *sdev, struct platform_device *pdev)
{
	struct device *dev;
	struct fb_info *info;
	struct fbtft_par *par;
	int ret;

	if (sdev) {
		dev = &sdev->dev;
		sainsmart32fb_display.txbuflen = 0;
	} else {
		dev = &pdev->dev;
		sainsmart32fb_display.txbuflen = -2; /* disable transmit buffer */
	}

	fbtft_init_dbg(dev, "%s()\n", __func__);

	info = fbtft_framebuffer_alloc(&sainsmart32fb_display, dev);
	if (!info)
		return -ENOMEM;

	par = info->par;
	if (sdev)
		par->spi = sdev;
	else
		par->pdev = pdev;

	par->fbtftops.init_display = sainsmart32fb_init_display;
	par->fbtftops.register_backlight = fbtft_register_backlight;
	par->fbtftops.write_reg = fbtft_write_reg16_bus8;
	par->fbtftops.set_addr_win = sainsmart32fb_set_addr_win;
	par->fbtftops.verify_gpios = sainsmart32fb_verify_gpios;

	if (pdev) {
		par->fbtftops.write_reg = fbtft_write_reg16_bus16;
		par->fbtftops.write_vmem = fbtft_write_vmem16_bus16;
		if (latched)
			par->fbtftops.write = fbtft_write_gpio16_wr_latched;
		else
			if (speed)
				par->fbtftops.write = write_gpio16_wr_slow;
			else
				par->fbtftops.write = fbtft_write_gpio16_wr;
	}

	ret = fbtft_register_framebuffer(info);
	if (ret < 0)
		goto out_release;

	return 0;

out_release:
	fbtft_framebuffer_release(info);

	return ret;
}

static int sainsmart32fb_remove_common(struct device *dev, struct fb_info *info)
{
	fbtft_init_dbg(dev, "%s()\n", __func__);

	if (info) {
		fbtft_unregister_framebuffer(info);
		fbtft_framebuffer_release(info);
	}

	return 0;
}

static int sainsmart32fb_probe_spi(struct spi_device *spi)
{
	return sainsmart32fb_probe_common(spi, NULL);
}

static int sainsmart32fb_remove_spi(struct spi_device *spi)
{
	struct fb_info *info = spi_get_drvdata(spi);

	return sainsmart32fb_remove_common(&spi->dev, info);
}

static int sainsmart32fb_probe_pdev(struct platform_device *pdev)
{
	return sainsmart32fb_probe_common(NULL, pdev);
}

static int sainsmart32fb_remove_pdev(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);

	return sainsmart32fb_remove_common(&pdev->dev, info);
}

static const struct spi_device_id sainsmart32fb_platform_ids[] = {
	{ "sainsmart32spifb", 0 },
	{ },
};

static struct spi_driver sainsmart32fb_spi_driver = {
	.driver = {
		.name   = DRVNAME,
		.owner  = THIS_MODULE,
	},
	.id_table = sainsmart32fb_platform_ids,
	.probe  = sainsmart32fb_probe_spi,
	.remove = sainsmart32fb_remove_spi,
};

static struct platform_driver sainsmart32fb_platform_driver = {
	.driver = {
		.name   = DRVNAME,
		.owner  = THIS_MODULE,
	},
	.probe  = sainsmart32fb_probe_pdev,
	.remove = sainsmart32fb_remove_pdev,
};

static int __init sainsmart32fb_init(void)
{
	int ret;

	ret = spi_register_driver(&sainsmart32fb_spi_driver);
	if (ret < 0)
		return ret;
	return platform_driver_register(&sainsmart32fb_platform_driver);
}

static void __exit sainsmart32fb_exit(void)
{
	spi_unregister_driver(&sainsmart32fb_spi_driver);
	platform_driver_unregister(&sainsmart32fb_platform_driver);
}

/* ------------------------------------------------------------------------- */

module_init(sainsmart32fb_init);
module_exit(sainsmart32fb_exit);

MODULE_DESCRIPTION("FB driver for the Sainsmart 3.2 LCD display");
MODULE_AUTHOR("Noralf Tronnes");
MODULE_LICENSE("GPL");
