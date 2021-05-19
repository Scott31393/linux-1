// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 NXP
 */

#include <dt-bindings/clock/imx8mn-clock.h>
#include <dt-bindings/power/imx8mn-power.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>

#include "blk-ctl.h"

#define MEDIA_BLK_SFT_EN_BUS_CLK_RSTN				BIT(8)
#define MEDIA_BLK_SFT_EN_ISI_CLK_RSTN				GENMASK(7, 6)
#define MEDIA_BLK_SFT_EN_LCDIF_CLK_RSTN			GENMASK(5, 4)
#define MEDIA_BLK_SFT_EN_MIPI_CSI_RSTN			GENMASK(3, 2)
#define MEDIA_BLK_SFT_EN_DSI_RSTN				GENMASK(1, 0)

#define MEDIA_BLK_BUS_PD_MASK					BIT(8)
#define MEDIA_BLK_ISI_PD_MASK					GENMASK(7,6)
#define MEDIA_BLK_LCDIF_PD_MASK				GENMASK(5, 4)
#define MEDIA_BLK_MIPI_CSI_PD_MASK				GENMASK(3, 2)
#define MEDIA_BLK_MIPI_DSI_PD_MASK				GENMASK(1, 0)

static struct imx_blk_ctl_hw imx8mn_dispmix_blk_ctl_pds[] = {
	IMX_BLK_CTL_PD("ISI", "dispmix", IMX8MN_BLK_CTL_DISPMIX_ISI, 0x4,
		       MEDIA_BLK_ISI_PD_MASK, 0, MEDIA_BLK_SFT_EN_ISI_CLK_RSTN,
		       IMX_BLK_CTL_PD_RESET),
	IMX_BLK_CTL_PD("LCDIF", "dispmix", IMX8MN_BLK_CTL_DISPMIX_LCDIF, 0x4,
		       MEDIA_BLK_LCDIF_PD_MASK, 0, MEDIA_BLK_SFT_EN_LCDIF_CLK_RSTN,
		       IMX_BLK_CTL_PD_RESET),
	IMX_BLK_CTL_PD("MIPI_DSI", "dispmix", IMX8MN_BLK_CTL_DISPMIX_MIPI_DSI, 0x4,
		       MEDIA_BLK_MIPI_DSI_PD_MASK, 0, MEDIA_BLK_SFT_EN_DSI_RSTN,
		       IMX_BLK_CTL_PD_RESET),
	IMX_BLK_CTL_PD("MIPI_CSI", "ISI", IMX8MN_BLK_CTL_DISPMIX_MIPI_CSI, 0x4,
		       MEDIA_BLK_MIPI_CSI_PD_MASK, 0, MEDIA_BLK_SFT_EN_MIPI_CSI_RSTN,
		       IMX_BLK_CTL_PD_RESET),
};

static const struct regmap_config imx8mn_blk_ctl_regmap_config = {
	.reg_bits		= 32,
	.reg_stride		= 4,
	.val_bits		= 32,
	.max_register		= 0x30,
	.fast_io		= true,
};

static const struct imx_blk_ctl_dev_data imx8mn_dispmix_blk_ctl_dev_data = {
	.pds = imx8mn_dispmix_blk_ctl_pds,
	.pds_num = ARRAY_SIZE(imx8mn_dispmix_blk_ctl_pds),
	.hw_hsk = IMX_BLK_CTL_PD(NULL, NULL, -1, 0x4, MEDIA_BLK_BUS_PD_MASK, 0,
				 MEDIA_BLK_SFT_EN_BUS_CLK_RSTN,
				 IMX_BLK_CTL_PD_HANDSHAKE | IMX_BLK_CTL_PD_RESET),
	.config = imx8mn_blk_ctl_regmap_config,
	.active_pd_names = (char*[]){"dispmix", "mipi"},
	.num_active_pd = 2,
};

static int imx8mn_blk_ctl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct imx_blk_ctl_dev_data *dev_data = of_device_get_match_data(dev);
	struct regmap *regmap;
	struct imx_blk_ctl *ctl;
	void __iomem *base;

	ctl = devm_kzalloc(dev, sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(dev, base, &dev_data->config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ctl->regmap = regmap;
	ctl->dev = dev;
	ctl->power_count = 0;
	mutex_init(&ctl->lock);

	ctl->num_clks = devm_clk_bulk_get_all(dev, &ctl->clks);
	if (ctl->num_clks < 0)
		return ctl->num_clks;

	dev_set_drvdata(dev, ctl);
	ctl->dev_data = dev_data;

	return imx_blk_ctl_register(dev);
}

static const struct of_device_id imx_blk_ctl_of_match[] = {
	{ .compatible = "fsl,imx8mn-dispmix-blk-ctl", .data = &imx8mn_dispmix_blk_ctl_dev_data },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_blk_ctl_of_match);

static struct platform_driver imx_blk_ctl_driver = {
	.probe = imx8mn_blk_ctl_probe,
	.driver = {
		.name = "imx8mn-blk-ctl",
		.of_match_table = of_match_ptr(imx_blk_ctl_of_match),
		.pm = &imx_blk_ctl_pm_ops,
	},
};
module_platform_driver(imx_blk_ctl_driver);
