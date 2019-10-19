/*
fpga_crc32 - example kernel driver for CRC32 in fpga fabric.
Copyright (C) 2019  James Bootsma <jrbootsma@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// General
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/wait.h>

// Hardware interaction
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

// Char device.
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

// HW Register definitions.
// Adapted from SDK-generated headers.
#define ADDR_AP_CTRL       0x00
#define AP_CTRL_START      (1ul << 0)
#define AP_CTRL_DONE       (1ul << 1)

#define ADDR_GIE           0x04
#define ADDR_IER           0x08
#define ADDR_ISR           0x0c
#define IRQ_DONE           (1ul << 0)
#define IRQ_READY          (1ul << 1)

#define ADDR_AP_RETURN     0x10
#define BITS_AP_RETURN     32
#define ADDR_MEM_DATA      0x18
#define BITS_MEM_DATA      32
#define ADDR_LEN_DATA      0x20
#define BITS_LEN_DATA      32
#define ADDR_INIT_VAL_DATA 0x28
#define BITS_INIT_VAL_DATA 32

// Module data.
static struct class * crc_class;

// Per-client data.
struct crc_file_data {
	// Only 1 file op at a time.
	struct mutex lock;
	// Keep intermediate crc val for each open handle.
	u32 current_crc;
};

// Per-device data.
struct crc_dev_data {
	// Clocks from device-tree.
	int num_clks;
	struct clk_bulk_data *clks;

	// Char-device stuff.
	dev_t devnum;
	struct cdev crc_dev;

	// Only one client can actually use the hardware at a time.
	struct mutex lock;

	// MMIO registers mapped to kernel space.
	void __iomem * hw_addr;
	// DMA memory physical address for peripheral to use.
	dma_addr_t dmem;
	// DMA memory virtual addres for kernel to use.
	char * kmem;

	// IRQ gotten from device-tree.
	int irqnum;
	// Allow to wait until interrupt.
	wait_queue_head_t wq;
};

static irqreturn_t crc_irq_handler(int irq, void * cookie)
{
	struct crc_dev_data * dev_data = cookie;
	// Clear all pending interrupts.
	writel(readl(dev_data->hw_addr + ADDR_ISR),
		dev_data->hw_addr + ADDR_ISR);
	wake_up(&dev_data->wq);
	return IRQ_HANDLED;
}

// Char-device methods.

static int crc_open(struct inode * inod, struct file * fp)
{
	int res = 0;
	struct crc_dev_data *dev_data =
		container_of(inod->i_cdev, struct crc_dev_data, crc_dev);
	struct crc_file_data * file_data = kzalloc(sizeof(*file_data), GFP_KERNEL);

	fp->private_data = file_data;

	if (!fp->private_data)
	{
		res = -ENOMEM;
		goto out;
	}

	mutex_init(&file_data->lock);

	res = clk_bulk_enable(dev_data->num_clks, dev_data->clks);

out:
	if (0 != res)
	{
		if (fp->private_data)
		{
			kfree(fp->private_data);
		}
	}

	return res;
}

static int crc_release(struct inode* inod, struct file * fp)
{
	struct crc_dev_data * data =
		container_of(inod->i_cdev, struct crc_dev_data, crc_dev);

	clk_bulk_disable(data->num_clks, data->clks);
	kfree(fp->private_data);
	return 0;
}

static ssize_t crc_read(
	struct file * fp,
	char __user * dst,
	size_t size,
	loff_t * offset)
{
	struct crc_file_data * file_data = fp->private_data;

	if( size > sizeof(u32) )
	{
		size = sizeof(u32);
	}

	mutex_lock(&file_data->lock);
	if (0 != copy_to_user(dst, &file_data->current_crc, size))
	{
		size = 0;
	}
	mutex_unlock(&file_data->lock);

	*offset += size;
	return size;
}

// Utility function for sending a single block of data to the peripheral
static bool handle_block(
	struct crc_dev_data * dev,
	struct crc_file_data * file,
	const char __user * src,
	size_t len
	)
{
	if (copy_from_user(dev->kmem, src, len))
		return false;

	// Setup to interupt on completion.
	writel(readl(dev->hw_addr + ADDR_ISR), dev->hw_addr + ADDR_ISR);
	writel(IRQ_DONE, dev->hw_addr + ADDR_IER);
	writel(1, dev->hw_addr + ADDR_GIE);

	// Setup the operation.
	writel((u32)dev->dmem, dev->hw_addr + ADDR_MEM_DATA);
	writel((u32)len, dev->hw_addr + ADDR_LEN_DATA);
	writel(~file->current_crc, dev->hw_addr + ADDR_INIT_VAL_DATA);
	writel(AP_CTRL_START, dev->hw_addr + ADDR_AP_CTRL);

	// Sleep until interrupt.
	wait_event(dev->wq, AP_CTRL_DONE & readl(dev->hw_addr + ADDR_AP_CTRL));

	// No more interrupts.
	writel(0, dev->hw_addr + ADDR_GIE);

	file->current_crc = ~readl(dev->hw_addr + ADDR_AP_RETURN);

	return true;
}

static ssize_t crc_write(
	struct file * fp,
	const char __user * src,
	size_t size,
	loff_t * offset)
{
	struct crc_dev_data * dev_data =
		container_of(fp->f_inode->i_cdev, struct crc_dev_data, crc_dev);
	struct crc_file_data * file_data = fp->private_data;
	size_t remaining = size;

	mutex_lock(&file_data->lock);
	mutex_lock(&dev_data->lock);
	{
		while (remaining) {
			size_t block_len = min_t(size_t, remaining, PAGE_SIZE);
			if (!handle_block(dev_data, file_data, src, block_len))
				break;
			remaining -= block_len;
			*offset += block_len;
			src += block_len;
		}
	}
	mutex_unlock(&dev_data->lock);
	mutex_unlock(&file_data->lock);

	return size - remaining;
}

static const char * crc_devname = "crc32_fpga";

static const struct file_operations crc_ops = {
	.owner = THIS_MODULE,
	.read = crc_read,
	.write = crc_write,
	.open = crc_open,
	.release = crc_release,
};

// Hardware setup.

static int crc_probe(struct platform_device * dev)
{
	bool clks_prepared = false;
	bool clks_enabled = false;
	bool dev_added = false;
	int result = 0;
	struct resource io_mem;

	struct crc_dev_data * dev_data =
		devm_kzalloc(&dev->dev, sizeof *dev_data, GFP_KERNEL);

	if (NULL == dev_data) {
		result = -ENOMEM;
		goto out;
	}

	mutex_init(&dev_data->lock);
	init_waitqueue_head(&dev_data->wq);
	platform_set_drvdata(dev, dev_data);

	// Clocks

	result = devm_clk_bulk_get_all(&dev->dev, &dev_data->clks);
	if (result < 0) {
		goto out;
	}
	dev_data->num_clks = result;

	result = clk_bulk_prepare(dev_data->num_clks, dev_data->clks);
	if (result) {
		goto out;
	}
	clks_prepared = true;

	// MMIO for control

	result = of_address_to_resource(dev->dev.of_node, 0, &io_mem);
	if (result) {
		goto out;
	}

	dev_data->hw_addr = devm_ioremap_resource(&dev->dev, &io_mem);
	if (IS_ERR(dev_data->hw_addr)) {
		result = PTR_ERR(dev_data->hw_addr);
		goto out;
	}

	// Setup memory for device to access.

	result = dma_set_mask_and_coherent(&dev->dev, DMA_BIT_MASK(32));
	if (result) {
		goto out;
	}

	dev_data->kmem = dmam_alloc_coherent(&dev->dev, PAGE_SIZE,
		&dev_data->dmem, GFP_KERNEL);
	if (IS_ERR(dev_data->kmem)) {
		result = PTR_ERR(dev_data->kmem);
		goto out;
	}

	// Start with interrupts disabled. Need clocks temporarily to
	// write control registers.

	result = clk_bulk_enable(dev_data->num_clks, dev_data->clks);
	if (result) {
		goto out;
	}
	clks_enabled = true;
	
	writel(0, dev_data->hw_addr + ADDR_GIE);

	clk_bulk_disable(dev_data->num_clks, dev_data->clks);
	clks_enabled = false;

	// Now setup actual interrupt handler.

	result = of_irq_get(dev->dev.of_node, 0);
	if (result < 0) {
		goto out;
	}
	dev_data->irqnum = result;

	result = devm_request_any_context_irq(&dev->dev, dev_data->irqnum,
		crc_irq_handler, 0, NULL, dev_data);
	if (result < 0) {
		goto out;
	}

	// Char device setup.

	cdev_init(&dev_data->crc_dev, &crc_ops);
	cdev_set_parent(&dev_data->crc_dev, &dev->dev.kobj);

	result = alloc_chrdev_region(&dev_data->devnum, 0, 1, crc_devname);
	if (result) {
		goto out;
	}

	result = cdev_add(&dev_data->crc_dev, dev_data->devnum, 1);

	if (result)
	{
		goto out;
	}
	dev_added = true;

	device_create(
		crc_class,
		&dev->dev,
		dev_data->devnum,
		NULL,
		"crc");

	result = 0;
out:
	if (result)
	{
		if (dev_data)
		{
			mutex_destroy(&dev_data->lock);
		}
		if (dev_added)
		{
			cdev_del(&dev_data->crc_dev);
		}
		if (dev_data && MAJOR(dev_data->devnum) != 0)
		{
			unregister_chrdev_region(dev_data->devnum, 1);
		}
		if (clks_enabled)
		{
			clk_bulk_disable(dev_data->num_clks, dev_data->clks);
		}
		if (clks_prepared)
		{
			clk_bulk_unprepare(dev_data->num_clks, dev_data->clks);
		}
	}

	return result;
}

static int crc_remove(struct platform_device *dev)
{
	struct crc_dev_data * dev_data = platform_get_drvdata(dev);
	// Most operations are devmanaged so don't need explicit cleanup.

	device_destroy(crc_class, dev_data->devnum);
	cdev_del(&dev_data->crc_dev);
	unregister_chrdev_region(dev_data->devnum, 1);
	clk_bulk_unprepare(dev_data->num_clks, dev_data->clks);
	mutex_destroy(&dev_data->lock);

	return 0;
}

// General metadata for module matching and loading.

static struct of_device_id devtable[] = {
	{ .compatible = "bootsma,fpga-crc32" },
	{},
};

static struct platform_driver crc_driver = {
	.probe = crc_probe,
	.remove = crc_remove,
	.driver = {
		.name = "fpga_crc32",
		.of_match_table = devtable,
	},
};

static int crc_init(void)
{
	crc_class = class_create(THIS_MODULE, "crc");
	if (NULL == crc_class)
	{
		return -ENOMEM;
	}

	return platform_driver_register(&crc_driver);
}
module_init(crc_init);

static void crc_cleanup(void)
{
	platform_driver_unregister(&crc_driver);

	class_destroy(crc_class);
	crc_class = NULL;
}
module_exit(crc_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Bootsma <jrbootsma@gmail.com>");
MODULE_DESCRIPTION("Driver for CRC32 example implemented in Zynq FPGA");
MODULE_DEVICE_TABLE(of, devtable);
