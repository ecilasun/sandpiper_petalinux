/*
 * ALSA sound driver for Sandpiper APU (Audio Processing Unit)
 *
 * This driver provides ALSA PCM playback support for the Sandpiper APU device.
 * The APU uses a ping-pong buffer architecture with DMA to play audio samples.
 *
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#define DRIVER_NAME "sandpiper_alsa"

/* APU Hardware Constants */
#define APU_BASE_ADDR           0x40000000
#define APU_REG_SIZE            0x1000

/* APU Commands */
#define APUCMD_BUFFERSIZE       0x00000000
#define APUCMD_START            0x00000001
#define APUCMD_NOOP             0x00000002
#define APUCMD_SWAPCHANNELS     0x00000003
#define APUCMD_SETRATE          0x00000004

/* Buffer Size Setting (1024 stereo samples = 4KB per buffer) */
#define APU_BUFFER_SIZE_CODE    5  /* x16 bursts = 1024 samples */
#define APU_SAMPLE_COUNT        1024
#define APU_BUFFER_BYTES        (APU_SAMPLE_COUNT * 4)  /* 4 bytes per stereo sample */

/* Sample Rate */
#define APU_SAMPLE_RATE         44100
#define APU_SAMPLE_RATE_CODE    0  /* 0 = 44.1 KHz */

/* ALSA PCM Hardware Parameters */
#define MIN_PERIODS             2
#define MAX_PERIODS             2
#define PERIOD_BYTES            APU_BUFFER_BYTES

/* Private data structure */
struct sandpiper_alsa {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct platform_device *pdev;
	void __iomem *apu_base;
	
	/* DMA buffers (ping-pong) */
	void *dma_area;
	dma_addr_t dma_addr;
	size_t dma_bytes;
	
	/* Playback state */
	struct snd_pcm_substream *substream;
	unsigned int hw_ptr;          /* Hardware pointer in frames */
	unsigned int buffer_pos;      /* Current buffer position (0 or 1) */
	unsigned int last_apu_frame;  /* Last read APU frame counter */
	
	/* Timer for period updates */
	struct hrtimer timer;
	ktime_t timer_period;
	atomic_t timer_running;
};

/* Helper functions to access APU registers */
static inline void apu_write_cmd(struct sandpiper_alsa *chip, u32 cmd)
{
	iowrite32(cmd, chip->apu_base);
}

static inline void apu_write_data(struct sandpiper_alsa *chip, u32 data)
{
	iowrite32(data, chip->apu_base);
}

static inline u32 apu_read_status(struct sandpiper_alsa *chip)
{
	return ioread32(chip->apu_base);
}

static inline u32 apu_get_frame(struct sandpiper_alsa *chip)
{
	return apu_read_status(chip) & 0x1;
}

/* Configure APU buffer size */
static void apu_set_buffer_size(struct sandpiper_alsa *chip)
{
	apu_write_cmd(chip, APUCMD_BUFFERSIZE);
	apu_write_data(chip, APU_BUFFER_SIZE_CODE);
}

/* Configure APU sample rate */
static void apu_set_sample_rate(struct sandpiper_alsa *chip)
{
	apu_write_cmd(chip, APUCMD_SETRATE);
	apu_write_data(chip, APU_SAMPLE_RATE_CODE);
}

/* Start DMA transfer to APU */
static void apu_start_dma(struct sandpiper_alsa *chip, dma_addr_t addr)
{
	apu_write_cmd(chip, APUCMD_START);
	apu_write_data(chip, (u32)addr);
}

/* Stop audio playback */
static void apu_stop(struct sandpiper_alsa *chip)
{
	apu_write_cmd(chip, APUCMD_SETRATE);
	apu_write_data(chip, 3);  /* 3 = Halt */
}

/* High-resolution timer callback - checks for buffer swaps */
static enum hrtimer_restart sandpiper_timer_callback(struct hrtimer *timer)
{
	struct sandpiper_alsa *chip = container_of(timer, struct sandpiper_alsa, timer);
	struct snd_pcm_runtime *runtime;
	u32 current_frame;
	
	if (!atomic_read(&chip->timer_running))
		return HRTIMER_NORESTART;
	
	if (!chip->substream || !chip->substream->runtime)
		goto restart_timer;
	
	runtime = chip->substream->runtime;
	
	/* Check if APU has swapped buffers */
	current_frame = apu_get_frame(chip);
	if (current_frame != chip->last_apu_frame) {
		chip->last_apu_frame = current_frame;
		
		/* APU has consumed a period, advance hardware pointer */
		chip->hw_ptr += APU_SAMPLE_COUNT;
		if (chip->hw_ptr >= runtime->buffer_size)
			chip->hw_ptr -= runtime->buffer_size;
		
		/* Submit next buffer to APU */
		chip->buffer_pos = (chip->buffer_pos + 1) & 1;
		dma_addr_t next_addr = chip->dma_addr + (chip->buffer_pos * APU_BUFFER_BYTES);
		apu_start_dma(chip, next_addr);
		
		/* Notify ALSA that a period has been processed */
		snd_pcm_period_elapsed(chip->substream);
	}
	
restart_timer:
	hrtimer_forward_now(timer, chip->timer_period);
	return HRTIMER_RESTART;
}

/* ALSA PCM operations */
static const struct snd_pcm_hardware sandpiper_pcm_hw = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID |
		 SNDRV_PCM_INFO_INTERLEAVED),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_44100,
	.rate_min = APU_SAMPLE_RATE,
	.rate_max = APU_SAMPLE_RATE,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = APU_BUFFER_BYTES * MAX_PERIODS,
	.period_bytes_min = PERIOD_BYTES,
	.period_bytes_max = PERIOD_BYTES,
	.periods_min = MIN_PERIODS,
	.periods_max = MAX_PERIODS,
};

static int sandpiper_pcm_open(struct snd_pcm_substream *substream)
{
	struct sandpiper_alsa *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	
	runtime->hw = sandpiper_pcm_hw;
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	chip->substream = substream;
	chip->hw_ptr = 0;
	chip->buffer_pos = 0;
	chip->last_apu_frame = apu_get_frame(chip);
	
	return 0;
}

static int sandpiper_pcm_close(struct snd_pcm_substream *substream)
{
	struct sandpiper_alsa *chip = snd_pcm_substream_chip(substream);
	
	/* Stop timer to prevent hardware from running after close */
	atomic_set(&chip->timer_running, 0);
	hrtimer_cancel(&chip->timer);
	
	/* Stop APU hardware */
	apu_stop(chip);
	
	chip->substream = NULL;
	return 0;
}

static int sandpiper_pcm_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct sandpiper_alsa *chip = snd_pcm_substream_chip(substream);
	size_t size = params_buffer_bytes(params);
	
	/* Allocate DMA buffer if needed */
	if (!chip->dma_area || chip->dma_bytes < size) {
		if (chip->dma_area) {
			dma_free_coherent(&chip->pdev->dev, chip->dma_bytes,
					  chip->dma_area, chip->dma_addr);
		}
		
		chip->dma_bytes = size;
		chip->dma_area = dma_alloc_coherent(&chip->pdev->dev, size,
						    &chip->dma_addr, GFP_KERNEL);
		if (!chip->dma_area) {
			dev_err(&chip->pdev->dev, "Failed to allocate DMA buffer\n");
			chip->dma_bytes = 0;
			return -ENOMEM;
		}
		
		/* Ensure 16-byte alignment as required by APU */
		if (chip->dma_addr & 0xF) {
			dev_err(&chip->pdev->dev, "DMA buffer not 16-byte aligned\n");
			dma_free_coherent(&chip->pdev->dev, size,
					  chip->dma_area, chip->dma_addr);
			chip->dma_area = NULL;
			chip->dma_bytes = 0;
			return -ENOMEM;
		}
		
		memset(chip->dma_area, 0, size);
	}
	
	substream->runtime->dma_area = chip->dma_area;
	substream->runtime->dma_addr = chip->dma_addr;
	substream->runtime->dma_bytes = size;
	
	return 0;
}

static int sandpiper_pcm_hw_free(struct snd_pcm_substream *substream)
{
	/* DMA buffer is managed by driver, not freed here */
	return 0;
}

static int sandpiper_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct sandpiper_alsa *chip = snd_pcm_substream_chip(substream);
	
	/* Reset hardware pointer and buffer position */
	chip->hw_ptr = 0;
	chip->buffer_pos = 0;
	chip->last_apu_frame = apu_get_frame(chip);
	
	/* Configure APU */
	apu_set_buffer_size(chip);
	apu_set_sample_rate(chip);
	
	return 0;
}

static int sandpiper_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct sandpiper_alsa *chip = snd_pcm_substream_chip(substream);
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		/* Start first DMA transfer */
		apu_start_dma(chip, chip->dma_addr);
		
		/* Start timer */
		atomic_set(&chip->timer_running, 1);
		hrtimer_start(&chip->timer, chip->timer_period, HRTIMER_MODE_REL);
		break;
		
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* Stop timer */
		atomic_set(&chip->timer_running, 0);
		hrtimer_cancel(&chip->timer);
		
		/* Stop APU */
		apu_stop(chip);
		break;
		
	default:
		return -EINVAL;
	}
	
	return 0;
}

static snd_pcm_uframes_t sandpiper_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct sandpiper_alsa *chip = snd_pcm_substream_chip(substream);
	
	return chip->hw_ptr;
}

static struct snd_pcm_ops sandpiper_pcm_ops = {
	.open = sandpiper_pcm_open,
	.close = sandpiper_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = sandpiper_pcm_hw_params,
	.hw_free = sandpiper_pcm_hw_free,
	.prepare = sandpiper_pcm_prepare,
	.trigger = sandpiper_pcm_trigger,
	.pointer = sandpiper_pcm_pointer,
};

/* Create PCM device */
static int sandpiper_pcm_new(struct sandpiper_alsa *chip)
{
	struct snd_pcm *pcm;
	int err;
	
	err = snd_pcm_new(chip->card, "Sandpiper APU", 0, 1, 0, &pcm);
	if (err < 0)
		return err;
	
	pcm->private_data = chip;
	strcpy(pcm->name, "Sandpiper APU PCM");
	chip->pcm = pcm;
	
	/* Set operators */
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &sandpiper_pcm_ops);
	
	return 0;
}

/* Platform driver probe */
static int sandpiper_alsa_probe(struct platform_device *pdev)
{
	struct snd_card *card;
	struct sandpiper_alsa *chip;
	int err;
	
	dev_info(&pdev->dev, "Sandpiper ALSA driver probe\n");
	
	/* Create sound card */
	err = snd_card_new(&pdev->dev, -1, "sandpiper", THIS_MODULE,
			   sizeof(struct sandpiper_alsa), &card);
	if (err < 0)
		return err;
	
	chip = card->private_data;
	chip->card = card;
	chip->pdev = pdev;
	
	/* Map APU registers */
	chip->apu_base = ioremap(APU_BASE_ADDR, APU_REG_SIZE);
	if (!chip->apu_base) {
		dev_err(&pdev->dev, "Failed to map APU registers\n");
		err = -ENOMEM;
		goto error;
	}
	
	/* Initialize high-resolution timer */
	hrtimer_init(&chip->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->timer.function = sandpiper_timer_callback;
	
	/* Timer period: check every ~2ms (much faster than the ~23.2ms period) */
	chip->timer_period = ktime_set(0, 2000000);  /* 2 milliseconds */
	atomic_set(&chip->timer_running, 0);
	
	/* Set DMA mask */
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	
	/* Create PCM device */
	err = sandpiper_pcm_new(chip);
	if (err < 0)
		goto error_unmap;
	
	/* Set card strings */
	strcpy(card->driver, "Sandpiper");
	strcpy(card->shortname, "Sandpiper APU");
	sprintf(card->longname, "Sandpiper Audio Processing Unit at 0x%08X",
		APU_BASE_ADDR);
	
	/* Register card */
	err = snd_card_register(card);
	if (err < 0)
		goto error_unmap;
	
	platform_set_drvdata(pdev, card);
	
	dev_info(&pdev->dev, "Sandpiper ALSA driver initialized successfully\n");
	return 0;
	
error_unmap:
	iounmap(chip->apu_base);
error:
	snd_card_free(card);
	return err;
}

static void sandpiper_alsa_remove(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);
	struct sandpiper_alsa *chip = card->private_data;
	
	/* Stop timer */
	atomic_set(&chip->timer_running, 0);
	hrtimer_cancel(&chip->timer);
	
	/* Stop APU */
	apu_stop(chip);
	
	/* Free DMA buffer */
	if (chip->dma_area) {
		dma_free_coherent(&pdev->dev, chip->dma_bytes,
				  chip->dma_area, chip->dma_addr);
	}
	
	/* Unmap registers */
	iounmap(chip->apu_base);
	
	/* Free card */
	snd_card_free(card);
	
	dev_info(&pdev->dev, "Sandpiper ALSA driver removed\n");
}

static struct of_device_id sandpiper_alsa_of_match[] = {
	{ .compatible = "sandpiper,apu", },
	{},
};
MODULE_DEVICE_TABLE(of, sandpiper_alsa_of_match);

static struct platform_driver sandpiper_alsa_driver = {
	.probe = sandpiper_alsa_probe,
	.remove = sandpiper_alsa_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sandpiper_alsa_of_match,
	},
};

module_platform_driver(sandpiper_alsa_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sandpiper Team");
MODULE_DESCRIPTION("ALSA sound driver for Sandpiper APU");
MODULE_ALIAS("platform:" DRIVER_NAME);
