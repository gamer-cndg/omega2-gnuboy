#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "pcm.h"
#include "rc.h"

/* omega includes. use ALSA for sound playback! */
#include <math.h>
#include <alsa/asoundlib.h>

#define SAMPLE_RATE (44100)

struct pcm pcm;

#define AUDIO_BUF_LEN (128)
//#define AUDIO_BUF_LEN (32*1024)
static byte buf[AUDIO_BUF_LEN];		/* main audio buffer */
static snd_pcm_t *handle;			/* main audio device handle */
static char *device = "default";	/* playback device */
rcvar_t pcm_exports[] =
{
	RCV_END
};


void pcm_init()
{
	pcm.hz = SAMPLE_RATE;
	pcm.buf = buf;
	pcm.len = sizeof buf;
	pcm.pos = 0;
	pcm.stereo = 1; //mono.

	printf("pcm_init()\n");

	//init ALSA
    int err;
    printf("Open device\n");
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            printf("Playback open error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
    }
    printf("Set params\n");
    if ((err = snd_pcm_set_params(handle,
                                  SND_PCM_FORMAT_U8,
                                  SND_PCM_ACCESS_RW_INTERLEAVED,
                                  2,
								  SAMPLE_RATE,
                                  1,
                                  500000)) < 0) {   /* 0.5sec */
            printf("Playback open error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
    }

    printf("[+] ALSA init okay.\n");
}

void pcm_close()
{
	memset(&pcm, 0, sizeof pcm);
    snd_pcm_close(handle);
}

int pcm_submit()
{
	if (!pcm.buf) return 0;
	if (pcm.pos < pcm.len) return 1;

	//printf("Sound fill.\n");

	size_t numFrames = sizeof(buf) / (pcm.stereo + 1);

    snd_pcm_sframes_t frames;
    frames = snd_pcm_writei(handle, buf, numFrames);
    if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
    if (frames < 0) {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            goto exit;
    }
    if (frames > 0 && frames < (long)numFrames)
            printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buf), frames);

exit:
	pcm.pos = 0;
	return 0;
}





