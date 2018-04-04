/*
 * sdl.c
 * sdl interfaces -- based on svga.c
 *
 * (C) 2001 Damian Gryski <dgryski@uwaterloo.ca>
 * Joystick code contributed by David Lau
 *
 * Licensed under the GPLv2, or later.
 */

#include <stdlib.h>
#include <stdio.h>

#include "fb.h"
#include "input.h"
#include "rc.h"

/* esp32 ili driver */
#include <stdint.h>
#include <string.h>
#include <ILI9225.h>
#include <pthread.h>
#include "queue.h"
/* time and clock libs for benchmarking */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


static void* render_thread();

static int frame_counter = 0;
static pthread_t videoThread;
void* queueBuffer[4];
queue_t queue = QUEUE_INITIALIZER(queueBuffer);


struct fb fb;

int useVideo;

rcvar_t vid_exports[] =
{
	RCV_BOOL("usevideo", &useVideo),
	RCV_END
};

static ili9225_initparams_t params  = {
	.mosi = 8,
	.sclk = 7,
	.cs = 6,
	.led = -1,
	.rst = 3,
	.rs = 1,
	.speedInHz = 80 * 1000000 //80MHz
};

void vid_init()
{
	//ILI initialization
	bool initOkay = ILI9225_Init(&params);
	if(!initOkay) {
		printf("[-] Failed to initialize ILI9255 display!\n");
		exit(-1);
	}

	ILI9225_FillFramebufferColor(COLOR_BLACK);
	ILI9225_CopyFramebuffer();

	fb.w = ILI9225_LCD_WIDTH;	//
	fb.h =  ILI9225_LCD_HEIGHT;
	fb.pelsize = 2; /* bytes per pixel */
	fb.pitch = ILI9225_LCD_WIDTH * 2; /* the length of a row of pixels in bytes. here width * 16 bit*/
	fb.indexed = fb.pelsize == 1;
	fb.ptr = ILI9225_GetRawFrameBuffer();
	fb.yuv = 0;
	fb.enabled = 1;

	/* we have a 16-bit R5G6R5 display! */
	fb.cc[0].r = 3;		/* shift amount right for red */
	fb.cc[0].l = 16-5;	/* shift amount left for red */
	fb.cc[1].r = 2;
	fb.cc[1].l = 16-5-6;
	fb.cc[2].r = 3;
	fb.cc[2].l = 16-5-6-5;

	//start render thread
	pthread_create(&videoThread, NULL, &render_thread, NULL);
}

void vid_setpal(int i, int r, int g, int b)
{
	printf("Setting index %d (%2X,%2X,%2X)\n", i,r,g,b);
}

void vid_preinit()
{
}

void vid_close()
{
	printf("Closing video.\n");
	int closeCMD = 0xFF;
	queue_enqueue(&queue, &closeCMD);
	pthread_join(videoThread, NULL);
	ILI9225_ClearFramebuffer();
	ILI9225_CopyFramebuffer();
	ILI9225_DeInit();
}

void vid_settitle(char *title)
{
	printf("vid_settitle: %s.\n", title);
}

void vid_begin()
{
}

/* local copy of buffer used by renderer
 * if we would not copy the framebuffer locally, the
 * buffer would be concurrently modified by the emulator,
 * which leads to tearing. */
static uint16_t localFrameBuf[ILI9225_LCD_WIDTH * ILI9225_LCD_HEIGHT];

static void* render_thread() {
	uint8_t* pFrameBuf = ILI9225_GetRawFrameBuffer();
	struct timespec start, end;

	while(1) {
		clock_gettime(CLOCK_MONOTONIC_RAW, &start);

		int* p = queue_dequeue(&queue);
		if(*p == 0xff) break;
		//make a local copy now.
		//this prevents tearing.
		memcpy(localFrameBuf, pFrameBuf, sizeof(localFrameBuf));
		//now that me made the copy, vid_end can continue
		queue_enqueue(&queue, p);
		//draw external frame buffer
		ILI9225_CopyExternalFrameBuffer((uint8_t*)localFrameBuf, sizeof(localFrameBuf));

		//benchmark
		clock_gettime(CLOCK_MONOTONIC_RAW, &end);
		uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
		int ms = (int)delta_us / 1000;
		//uncomment this if you want to measure FPS
		static int framecount = 0;
		static int framelen = 0;
		static const int frameLimit = 30;
		framelen += ms;
		if(++framecount == frameLimit) {
			float avg_ms = framelen / 30.0f;
			printf("Frame time %.2f ms (FPS: %.3f)\n", avg_ms, (1000.0f/avg_ms));
			framecount = 0;
			framelen = 0;
		}
	}
	return NULL;
}

void vid_end()
{
	frame_counter++;
	//signal the render thread make a copy
	int someVal = 123;
	queue_enqueue(&queue, &someVal);
	//force a context switch by sleeping for a 1ms.
	//this gives the renderer the time to lock on the condition object
	usleep(1 * 1000);
	//wait until render thread has copied framebuffer.
	//we do this on the same queue - if the renderer is still busy
	//drawing the frame, we will receive the value again and emulation continues.
	//queue_enqueue does a broadcast which will already notify the render thread.
	int* dummy = queue_dequeue(&queue);
	(void)dummy;
}
