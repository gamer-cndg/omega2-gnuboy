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

/* available video output modes */
#define GNUBOY_USE_FB0		1
#define GNUBOY_USE_LIBDLO	2
#define GNUBOY_USE_ILI9225	3

/* select video method */
#define GNUBOY_VIDEO_METHOD	GNUBOY_USE_LIBDLO

#define GBC_LCD_WIDTH	160
#define GBC_LCD_HEIGHT	144

#if 0
#define EMULATED_VIDEO_WIDTH	640
#define EMULATED_VIDEO_HEIGHT	480
#else
#define EMULATED_VIDEO_WIDTH	GBC_LCD_WIDTH
#define EMULATED_VIDEO_HEIGHT	GBC_LCD_HEIGHT
#endif

/* standard libraries for queues and threads */
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "queue.h"

/* time and clock libs for benchmarking */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if GNUBOY_VIDEO_METHOD == GNUBOY_USE_ILI9225
#include <ILI9225.h>

static void* render_thread();

static int frame_counter = 0;
static pthread_t videoThread;
void* queueBuffer[4];
queue_t queue = QUEUE_INITIALIZER(queueBuffer);

static ili9225_initparams_t params = {
	.mosi = 8,
	.sclk = 7,
	.cs = 6,
	.led = -1,
	.rst = 3,
	.rs = 1,
	.speedInHz = 80 * 1000000 //80MHz
};
#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_FB0

typedef uint_fast16_t uint;

typedef struct {
	char *buffer;
	size_t size,
	bytes_per_pixel, bytes_per_line,
	width, height;
	uint red, green, blue;
}Screen;

typedef struct {
	uint_fast8_t r, g, b, a;
}Color;

#define Die(Msg, ...) { \
    fprintf (stderr, "fbgrad: " Msg ".\n", __VA_ARGS__); \
    exit(1); \
}\

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define fbdev "/dev/fb0"
static int fbfd;
static uint8_t* realFrameBuffer = NULL;
static uint16_t fakeFrameBuf[/*240 * 320*/160 * 144 ];

#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_LIBDLO

#include <libdlo.h>

/* macros for error handling and other stuff */

/** Source file name for the last recorded error event. */
char err_file[1024];

/** Source line number for last recorded error event. */
uint32_t err_line;

/** Record the line and source file associated with a particular type of error event. */
#define REC_ERR() { snprintf(&err_file[0], 1024, "%s", __FILE__); err_line = __LINE__; }

/** If a called function (cmd) returns an error code, return the error code from the calling function. */
#define ERR(cmd) do { dlo_retcode_t __err = (cmd); if (__err != dlo_ok) return; } while(0)

/** If a usb_ function call returns an error code, return an error and store the code in the @a usberr global. */
#define UERR(cmd) do { usberr = (cmd); if (usberr < 0) return usb_error_grab(); } while (0)

/** If a memory-allocating call returns NULL, return with a memory allocation error code. */
#define NERR(ptr) do { if (!(ptr)) { REC_ERR(); return; } } while (0)

/** If a function call (cmd) returns an error code, jump to the "error" label. Requires variable declaration: dlo_retcode_t err; */
#define ERR_GOTO(cmd) do { err = (cmd); if (err != dlo_ok) goto error; } while(0)

/** If a usb_ function call returns an error code, jump to the "error" label and store the usb error code in the @a usberr global. */
#define UERR_GOTO(cmd) do { usberr = (cmd); if (usberr < 0) { err = usb_error_grab(); goto error; } } while(0)

/** If a memory-allocating call returns NULL, jump to the "error" label with a memory allocation error code. */
#define NERR_GOTO(ptr) do { if (!(ptr)) { REC_ERR(); err = dlo_err_memory; goto error; } } while(0)

/* Video device handle */
dlo_dev_t uid = 0;

/* Viewport of the device */
dlo_view_t view;

/* framebuffer */
uint8_t* dloFramebuf = NULL;

#else
#error "Invalid video output mode selected"
#endif

struct fb fb;

int useVideo;

rcvar_t vid_exports[] = {
RCV_BOOL("usevideo", &useVideo),
RCV_END };

/* test */
typedef uint_fast16_t uint;
typedef struct {
	char *buffer;
	size_t size, bytes_per_pixel, bytes_per_line, width, height;
	uint red, green, blue;
} Screen;
typedef struct {
	uint_fast8_t r, g, b, a;
} Color;
#define Die(Msg, ...) { \
    fprintf (stderr, "fbgrad: " Msg ".\n", __VA_ARGS__); \
    exit(1); \
}\

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

void vid_init() {
	int w = 0, h = 0, pitch = 0, indexed = 0;
	byte* framebuf = NULL;
#if GNUBOY_VIDEO_METHOD == GNUBOY_USE_FB0
	fbfd = open (fbdev, O_RDWR);
	if (fbfd < 0)
	Die ("cannot open \"%s\"", fbdev);

	struct fb_var_screeninfo vinf;
	struct fb_fix_screeninfo finf;

	if (ioctl (fbfd, FBIOGET_FSCREENINFO, &finf) == -1)
	Die ("cannot open fixed screen info for \"%s\"", fbdev);

	if (ioctl (fbfd, FBIOGET_VSCREENINFO, &vinf) == -1)
	Die ("cannot open variable screen info for \"%s\"", fbdev);

	Screen s = {
		.size = finf.line_length * vinf.yres,
		.bytes_per_pixel = vinf.bits_per_pixel / 8,
		.bytes_per_line = finf.line_length,
		.red = vinf.red.offset/8,
		.green = vinf.green.offset/8,
		.blue = vinf.blue.offset/8,
		.width = vinf.xres,
		.height = vinf.yres
	};

	s.buffer = mmap (0, s.size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

	if (s.buffer == MAP_FAILED)
	Die ("cannot map frame buffer \"%s\"", fbdev);

	realFrameBuffer = (uint8_t*) s.buffer;

#if USE_DOUBLE_COPY
	w = 160; //s.width;
	h = 144;//s.height;
	pitch = w * 2;//(s.bytes_per_line);
	framebuf = (uint8_t*) fakeFrameBuf;;
#else
	w = s.width;
	h = s.height;
	pitch = (s.bytes_per_line);
	framebuf = (uint8_t*) s.buffer;
	indexed = 1;
#endif

	printf("w = %d h = %d pitch = %d\n", w, h, pitch);
#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_ILI9225
	//ILI initialization
	bool initOkay = ILI9225_Init(&params);
	if(!initOkay) {
		printf("[-] Failed to initialize ILI9255 display!\n");
		exit(-1);
	}

	ILI9225_FillFramebufferColor(COLOR_BLACK);
	ILI9225_CopyFramebuffer();
	w = ILI9225_LCD_WIDTH;
	h = ILI9225_LCD_HEIGHT;
	pitch = ILI9225_LCD_WIDTH * 2;
	indexed = fb.pelsize == 1;
	byte = /* ILI9255_Get_Raw_Framebuf()*/

	//start render thread
	pthread_create(&videoThread, NULL, &render_thread, NULL);
#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_LIBDLO

	/* initialize the DisplayLink device */

	dlo_init_t ini_flags = { 0 };
	//dlo_final_t fin_flags = { 0 };
	dlo_claim_t cnf_flags = { 0 };
	dlo_retcode_t err;

	ERR_GOTO(dlo_init(ini_flags));

	/* Look for a DisplayLink device to connect to */
	uid = dlo_claim_first_device(cnf_flags, 0);

	if (!uid) {
		//error
		printf("Failed to claim device..\n");
		return;
	}

	//dlo_mode_t mode;
	dlo_mode_t *mode_info;
	dlo_devinfo_t *dev_info;

	/* Read some information on the device */
	dev_info = dlo_device_info(uid);
	NERR(dev_info);
	printf("test: device info: uid &%X\n", (int) dev_info->uid);
	printf("test: device info: serial %s\n", dev_info->serial);
	printf("test: device info: type  &%X\n", (uint32_t) dev_info->type);

	/* Read current mode information (if we're already in the display's native mode) */
	mode_info = dlo_get_mode(uid);
	NERR(mode_info);
	printf("test: native mode info...\n");
	printf("  %ux%u @ %u Hz %u bpp base &%X\n", mode_info->view.width,
			mode_info->view.height, mode_info->refresh, mode_info->view.bpp,
			(int) mode_info->view.base);

	/* Select the monitor's preferred mode, based on EDID */
	printf("test: set_mode...\n");
	//ERR(dlo_set_mode(uid, NULL));

	//Select custom resolution
	dlo_mode_t desc;
	desc.refresh = 60;
	desc.view.base = 0;
	desc.view.width = 640;
	desc.view.height = 480;
	desc.view.bpp = 24;

	ERR(dlo_set_mode(uid, &desc));

	/* Read current mode information */
	mode_info = dlo_get_mode(uid);
	NERR(mode_info);
	printf("test: mode info...\n");
	printf("  %ux%u @ %u Hz %u bpp base &%X\n", mode_info->view.width,
			mode_info->view.height, mode_info->refresh, mode_info->view.bpp,
			(int) mode_info->view.base);
	view = (mode_info->view);

	dlo_fill_rect(uid, NULL, NULL, DLO_RGB(0, 0, 0));

	//allocate an RGB16 fraembuffer

	w = EMULATED_VIDEO_WIDTH;
	h = EMULATED_VIDEO_HEIGHT;

	size_t numFramebufBytes = w * h * 2;
	dloFramebuf = calloc(1, numFramebufBytes);
	if (!dloFramebuf) {
		printf("Failed to allocate framebuffer\n");
		return;
	}

	pitch = w * 2;
	framebuf = dloFramebuf;

	printf("DisplayLink Init OK\n");

	//hangup on purpose
	if (0) {
		printf("Hanging..\n");
		//while(1) usleep(100 * 1000UL);
		int fbfd;
		const char* fbdev = "/dev/fb0";
		fbfd = open(fbdev, O_RDWR);
		if (fbfd < 0)
			Die("cannot open \"%s\"", fbdev);

		struct fb_var_screeninfo vinf;
		struct fb_fix_screeninfo finf;

		if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finf) == -1)
			Die("cannot open fixed screen info for \"%s\"", fbdev);

		if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinf) == -1)
			Die("cannot open variable screen info for \"%s\"", fbdev);

		Screen s = { .size = finf.line_length * vinf.yres, .bytes_per_pixel =
				vinf.bits_per_pixel / 8, .bytes_per_line = finf.line_length,
				.red = vinf.red.offset / 8, .green = vinf.green.offset / 8,
				.blue = vinf.blue.offset / 8, .width = vinf.xres, .height =
						vinf.yres };

		s.buffer = mmap(0, s.size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

		if (s.buffer == MAP_FAILED)
			Die("cannot map frame buffer \"%s\"", fbdev);

		printf("Got frame buffer. Writing now.\n");
		sleep(2);
		memset(s.buffer, s.size, 0xff);
		while (1)
			usleep(100 * 1000UL);
	}

#endif

	fb.w = w;
	fb.h = h;
	fb.pelsize = 2; /* bytes per pixel */
	fb.pitch = pitch; /* the length of a row of pixels in bytes. here width * 16 bit*/
	fb.indexed = fb.pelsize == 1;
	fb.ptr = framebuf;
	fb.yuv = 0;
	fb.enabled = 1;

	/* we have a 16-bit R5G6R5 display! */
	fb.cc[0].r = 3; /* shift amount right for red */
	fb.cc[0].l = 16 - 5; /* shift amount left for red */
	fb.cc[1].r = 2;
	fb.cc[1].l = 16 - 5 - 6;
	fb.cc[2].r = 3;
	fb.cc[2].l = 16 - 5 - 6 - 5;

	return;

	error:
	/* some error codes */
	printf("DisplayLink Error %u %s\n", (int) err, dlo_strerror(err));
	return;
}

void vid_setpal(int i, int r, int g, int b) {
	printf("Setting index %d (%2X,%2X,%2X)\n", i, r, g, b);
}

void vid_preinit() {
}

void vid_close() {
#if GNUBOY_VIDEO_METHOD == GNUBOY_USE_ILI9225
	printf("Closing video.\n");
	int closeCMD = 0xFF;
	queue_enqueue(&queue, &closeCMD);
	pthread_join(videoThread, NULL);
	ILI9225_ClearFramebuffer();
	ILI9225_CopyFramebuffer();
	ILI9225_DeInit();
#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_FB0
	close(fbfd);
#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_LIBDLO
	dlo_fill_rect(uid, NULL, NULL, DLO_RGB(0, 0, 0));
	printf("release DisplayLink handle &%X...\n", (uintptr_t) uid);
	dlo_release_device(uid);
	if (dloFramebuf != NULL)
		free(dloFramebuf);
	dloFramebuf = NULL;
	dlo_final_t flags = { 0 };
	dlo_final(flags);
#endif
}

void vid_settitle(char *title) {
	printf("vid_settitle: %s.\n", title);
}

void vid_begin() {
}

#if GNUBOY_VIDEO_METHOD == GNUBOY_USE_ILI9225

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

#endif

struct timespec measureStartTime;
static const int frameLimit = 120;
static int framecount = 0;

void vid_end() {
	if (framecount == 0) {
		//start measurement
		clock_gettime(CLOCK_MONOTONIC_RAW, &measureStartTime);
	}
	framecount++;
	if (framecount == frameLimit) {

		struct timespec measureEndTime;
		clock_gettime(CLOCK_MONOTONIC_RAW, &measureEndTime);
		uint64_t delta_us = ((measureEndTime.tv_sec - measureStartTime.tv_sec)
				* 1000000)
				+ ((measureEndTime.tv_nsec - measureStartTime.tv_nsec) / 1000);

		float ms = (int) delta_us / 1000.0 / frameLimit;
		printf("FPS: %.3f delta micros %u\n", 1000.0 / ms,
				(unsigned) (delta_us / frameLimit));
		framecount = 0;
	}
#if GNUBOY_VIDEO_METHOD == GNUBOY_USE_FB0
	//memcpy(realFrameBuffer, fakeFrameBuf, sizeof(fakeFrameBuf));

#elif GNUBOY_VIDEO_METHOD == GNUBOY_USE_ILI9225
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
#else

	//render framebuffer here..
	if (!dloFramebuf)
		return;

#ifdef GNUBOY_DOUBLE_SCALE
	//copy this framebuffer (2 bytes per pixel) into a bigger framebuffer
	static uint16_t* bigFrameBuf;
	if(bigFrameBuf == NULL)
	bigFrameBuf = calloc(EMULATED_VIDEO_HEIGHT * 2 * EMULATED_VIDEO_WIDTH * 2, sizeof(uint16_t));

#define PIXEL_INDEX(X, Y, STRIDE) ((Y)*(STRIDE)+(X))

	uint16_t* sourceBuf = (uint16_t*) dloFramebuf;
	for(int y = 0; y < EMULATED_VIDEO_HEIGHT; y++) {
		for(int x = 0; x < EMULATED_VIDEO_WIDTH; x++) {
			int pixelPosSource = PIXEL_INDEX(x, y, EMULATED_VIDEO_WIDTH);
			uint16_t p = sourceBuf[pixelPosSource];
			bigFrameBuf[PIXEL_INDEX(2*x, 2*y, EMULATED_VIDEO_WIDTH * 2)] = p;
			bigFrameBuf[PIXEL_INDEX(2*x+1, 2*y, EMULATED_VIDEO_WIDTH * 2)] = p;
			bigFrameBuf[PIXEL_INDEX(2*x, 2*y+1, EMULATED_VIDEO_WIDTH * 2)] = p;
			bigFrameBuf[PIXEL_INDEX(2*x+1,2*y+1, EMULATED_VIDEO_WIDTH * 2)] = p;

		}
	}
#endif

	dlo_bmpflags_t flags = { 0 };
	dlo_fbuf_t fbuf = { 0 };
	dlo_dot_t dot;
	dlo_retcode_t err = 0;

#ifdef GNU_BOY_DOUBLE_SCALE
	fbuf.width = EMULATED_VIDEO_WIDTH * 2;
	fbuf.height = EMULATED_VIDEO_HEIGHT * 2;
	fbuf.base = bigFrameBuf;
#else
	fbuf.width = EMULATED_VIDEO_WIDTH;
	fbuf.height = EMULATED_VIDEO_HEIGHT;
	fbuf.base = dloFramebuf;
#endif
	fbuf.fmt = dlo_pixfmt_rgb565;
	fbuf.stride = fbuf.width;

	dot.x = 640 / 2 - (fbuf.width / 2);
	dot.y = 480 / 2 - (fbuf.height / 2);

	//printf("Pushing FBf framebuf %p\n", dloFramebuf);
	err = dlo_copy_host_bmp(uid, flags, &fbuf, &view, &dot);
	if (err != dlo_ok) {
		printf("Pushing framebuffer failed %u %s\n", (int) err,
				dlo_strerror(err));
	} else {
		//printf("Pushed FB\n");
	}

#endif
}
