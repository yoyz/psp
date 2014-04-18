#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <string.h>
#include <stdio.h>

//#include <tremor/ivorbiscodec.h>
//#include <tremor/ivorbisfile.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <FLAC/stream_decoder.h>

#define printf pspDebugScreenPrintf

#define VERS	 1
#define REVS	 0

extern char *RequestFile(char *);

PSP_MODULE_INFO("OggPlayer", 0, VERS, REVS);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();

#define OUTPUT_BUFFER 32768  // must be less than PSP_AUDIO_SAMPLE_MAX*4

/////////////////////////////////////////////////////////////////////////////////////////
//Globals
/////////////////////////////////////////////////////////////////////////////////////////

int runningFlag;
int bufferEmpty;
int bufferFull;

int audioVol = PSP_AUDIO_VOLUME_MAX;

char pcmout1[OUTPUT_BUFFER];
char pcmout2[OUTPUT_BUFFER];
long pcmlen1, pcmlen2;
int bufferFlip = 0;

// Ogg variables

OggVorbis_File OGG_VorbisFile;
int OGG_eos = 0;
int OGG_audio_channel = 0;
int bitStream;
FILE *the_file = 0;

// FLAC variables

FLAC__uint64 total_samples = 0;
unsigned int sample_rate = 0;
unsigned int channels = 0;
unsigned int bps = 0;

/////////////////////////////////////////////////////////////////////////////////////////
//Callbacks
/////////////////////////////////////////////////////////////////////////////////////////

/* Exit callback */
int exit_callback(int arg1, int arg2, void *common) {
	sceKernelExitGame();
	return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp) {
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void) {
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if(thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}


/////////////////////////////////////////////////////////////////////////////////////////
//FLAC callbacks
/////////////////////////////////////////////////////////////////////////////////////////

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
//	FILE *f = (FILE*)client_data;
//	const FLAC__uint32 total_size = (FLAC__uint32)(total_samples * channels * (bps/8));
	int i;
	FLAC__int16 *fillbuf;
	SceCtrlData pad;

	(void)decoder, (void)client_data;

	if(channels != 2 || bps != 16)
	{
		printf(" ERROR: this player only supports 16bit stereo streams\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	/* copy decoded PCM samples to buffer */
	sceKernelWaitSema(bufferEmpty, 1, 0);

	fillbuf = bufferFlip ? (FLAC__int16 *)pcmout2 : (FLAC__int16 *)pcmout1;
	for (i=0; i<frame->header.blocksize; i++)
	{
		fillbuf[i<<1] = (FLAC__int16)buffer[0][i];
		fillbuf[(i<<1)+1] = (FLAC__int16)buffer[1][i];
	}
	if (bufferFlip)
		pcmlen2 = frame->header.blocksize<<2;
	else
		pcmlen1 = frame->header.blocksize<<2;
	sceKernelSignalSema(bufferFull, 1);

	sceCtrlReadBufferPositive(&pad, 1);
	if(pad.Buttons & PSP_CTRL_CROSS)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if (pad.Buttons & PSP_CTRL_LTRIGGER)
		if (audioVol > 0)
			audioVol -= 0x0800; // 16 steps on the audio

	if (pad.Buttons & PSP_CTRL_RTRIGGER)
		if (audioVol < 0x8000)
			audioVol += 0x0800; // 16 steps on the audio

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	int secs = 0;
	int h = 0;
	int m = 0;
	int s = 0;
	char dest[9];

	(void)decoder, (void)client_data;

	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		total_samples = metadata->data.stream_info.total_samples;
		sample_rate = metadata->data.stream_info.sample_rate;
		channels = metadata->data.stream_info.channels;
		bps = metadata->data.stream_info.bits_per_sample;

		printf(" sample rate    : %u Hz\n", sample_rate);
		printf(" channels       : %u\n", channels);
		printf(" bits per sample: %u\n", bps);

		secs = total_samples/sample_rate;
		h = secs / 3600;
		m = (secs - h * 3600) / 60;
		s = secs - h * 3600 - m * 60;
		snprintf(dest, sizeof(dest), "%2.2i:%2.2i:%2.2i", h, m, s);
		printf(" length         : %s\n", dest);
	}
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	printf(" Error: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

/////////////////////////////////////////////////////////////////////////////////////////
//Buffer filling
/////////////////////////////////////////////////////////////////////////////////////////

int fillBuffer(SceSize args, void *argp)
{
	int bytes, bytesRed;
	int fillbuf;

	while(runningFlag){
		sceKernelWaitSema(bufferEmpty, 1, 0);
		if (OGG_eos || !runningFlag)
			break;
		bytesRed = 0;
		fillbuf = bufferFlip ? (int)pcmout2 : (int)pcmout1;
		while (bytesRed < OUTPUT_BUFFER && runningFlag && !OGG_eos)
		{
			//bytes = ov_read(&OGG_VorbisFile,(char *)(fillbuf+bytesRed),OUTPUT_BUFFER-bytesRed,&bitStream);
			bytes = ov_read(&OGG_VorbisFile,(char *)(fillbuf+bytesRed),OUTPUT_BUFFER-bytesRed,0,2,1,&bitStream);
			if (bytes == 0)
			{
				//EOF:
				OGG_eos = 1;
				ov_clear(&OGG_VorbisFile);
			}
			else if (bytes > 0)
			{
				bytesRed += bytes;
			}
		}
		if (bufferFlip)
			pcmlen2 = bytesRed;
		else
			pcmlen1 = bytesRed;
		sceKernelSignalSema(bufferFull, 1);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
//Audio output
/////////////////////////////////////////////////////////////////////////////////////////

int audioOutput(SceSize args, void *argp)
{
	int playlen;
	char *playbuf;

	while(runningFlag)
	{
		sceKernelWaitSema(bufferFull, 1, 0);
		if (OGG_eos || !runningFlag)
			break;
		playlen = bufferFlip ? pcmlen1 : pcmlen2;
		playbuf = bufferFlip ? pcmout1 : pcmout2;
		bufferFlip ^= 1;
		sceKernelSignalSema(bufferEmpty, 1);
		sceAudioSetChannelDataLen(OGG_audio_channel, playlen/4);
		sceAudioOutputBlocking(OGG_audio_channel, audioVol, playbuf);
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
//Main
/////////////////////////////////////////////////////////////////////////////////////////

int main() {
	char *filename;
	SceCtrlData pad;
	char dest[9];
	long secs;
	int h;
	int m;
	int s;
	int audioThid;
	vorbis_info *vi;
	FLAC__bool ok = true;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;

	pspDebugScreenInit();
	SetupCallbacks();

	//Creates semaphores:
	bufferEmpty = sceKernelCreateSema("bufferEmpty", 0, 1, 1, 0);
	bufferFull = sceKernelCreateSema("bufferFull", 0, 0, 1, 0);

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xA0602000);
	pspDebugScreenSetTextColor(0xffffff00);

	audioVol = 0x4000; // half max volume

mloop:
	filename = RequestFile("ms0:/MUSIC/");

	pspDebugScreenClear();
	pspDebugScreenSetXY(25, 1);
	printf("Simple Ogg Player");
	pspDebugScreenSetXY(25, 3);
	printf("CPU: %i BUS: %i", scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency());
	pspDebugScreenSetXY(21, 4);
	printf("Press X to stop playback");

	printf("\n\n Playing %s\n\n", filename);
	sceKernelDelayThread(2*1000*1000);

	if ( !strcmp(&filename[strlen(filename)-3], "ogg") )
		goto play_ogg;

// play_flac:
	if((decoder = FLAC__stream_decoder_new()) == NULL)
	{
		printf(" Could not make FLAC decoder. FLAC files cannot be played.\n");
		sceKernelDelayThread(2*1000*1000);
		goto mloop;
	}

	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status = FLAC__stream_decoder_init_file(decoder, filename, write_callback, metadata_callback, error_callback, the_file);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		printf(" ERROR: initializing decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		fclose(the_file);
		sceKernelDelayThread(2*1000*1000);
		goto mloop;
	}

	memset(pcmout1, 0, OUTPUT_BUFFER);
	memset(pcmout2, 0, OUTPUT_BUFFER);
	pcmlen1 = 0;
	pcmlen2 = 0;

	OGG_eos = 0;
	runningFlag = 1;
	sceKernelSignalSema(bufferEmpty, 1);
	sceKernelSignalSema(bufferFull, 0);

	//Reserve audio channel:
	OGG_audio_channel = sceAudioChReserve(OGG_audio_channel, OUTPUT_BUFFER/4, PSP_AUDIO_FORMAT_STEREO);

	//Start audio output thread:
	audioThid = sceKernelCreateThread("audioOutput", audioOutput, 0x16, 0x1800, PSP_THREAD_ATTR_USER, NULL);
	if(audioThid < 0)
		sceKernelExitGame();
	sceKernelStartThread(audioThid, 0, NULL);

	ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);
	printf(" decoding: %s\n", ok ? "succeeded" : "FAILED");
	printf("    state: %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder)]);

	FLAC__stream_decoder_delete(decoder);
	fclose(the_file);

	runningFlag = 0; // kill the threads
	sceKernelSignalSema(bufferEmpty, 1);
	sceKernelSignalSema(bufferFull, 1);
	sceKernelDelayThread(1*1000*1000);

	//Release channel:
	sceAudioChRelease(OGG_audio_channel);

	sceKernelDelayThread(1*1000*1000);
	goto mloop;

play_ogg:
	//Apro il file OGG:
	the_file = fopen(filename, "r");
	if ((the_file) != NULL)
		ov_open(the_file, &OGG_VorbisFile, NULL, 0);
	else
	{
		printf(" Error opening file\n");
		sceKernelDelayThread(2*1000*1000);
		goto mloop;
	}

	//Stampo le informazioni sul file:
	vi = ov_info(&OGG_VorbisFile, -1);
	printf(" Channels   : %d\n", vi->channels);
	printf(" Sample rate: %ld Hz\n", vi->rate);
	printf(" Bitrate    : %ld kBit\n", vi->bitrate_nominal/1000);

	h = 0;
	m = 0;
	s = 0;
	secs = (long)ov_time_total(&OGG_VorbisFile, -1)/1000;
	h = secs / 3600;
	m = (secs - h * 3600) / 60;
	s = secs - h * 3600 - m * 60;
	snprintf(dest, sizeof(dest), "%2.2i:%2.2i:%2.2i", h, m, s);
	printf(" Length	: %s\n", dest);

	sceKernelDelayThread(2*1000*1000);

	memset(pcmout1, 0, OUTPUT_BUFFER);
	memset(pcmout2, 0, OUTPUT_BUFFER);
	pcmlen1 = 0;
	pcmlen2 = 0;

	OGG_eos = 0;
	runningFlag = 1;
	sceKernelSignalSema(bufferEmpty, 1);
	sceKernelSignalSema(bufferFull, 0);

	//Reserve audio channel:
	OGG_audio_channel = sceAudioChReserve(OGG_audio_channel, OUTPUT_BUFFER/4, PSP_AUDIO_FORMAT_STEREO);

	//Start buffer filling thread:
	int bufferThid = sceKernelCreateThread("bufferFilling", fillBuffer, 0x12, 0x1800, PSP_THREAD_ATTR_USER, NULL);
	if(bufferThid < 0)
		sceKernelExitGame();
	sceKernelStartThread(bufferThid, 0, NULL);

	//Start audio output thread:
	audioThid = sceKernelCreateThread("audioOutput", audioOutput, 0x16, 0x1800, PSP_THREAD_ATTR_USER, NULL);
	if(audioThid < 0)
		sceKernelExitGame();
	sceKernelStartThread(audioThid, 0, NULL);

	while(runningFlag && !OGG_eos)
	{
		//Print timestring:
		secs = (long) ov_time_tell(&OGG_VorbisFile)/1000;
		h = secs / 3600;
		m = (secs - h * 3600) / 60;
		s = secs - h * 3600 - m * 60;
		snprintf(dest, sizeof(dest), "%2.2i:%2.2i:%2.2i", h, m, s);
		pspDebugScreenSetXY(1, 16);
		printf("Current    : %s\n", dest);

		sceCtrlReadBufferPositive(&pad, 1);
		if(pad.Buttons & PSP_CTRL_CROSS)
			break; // stop playback

		if (pad.Buttons & PSP_CTRL_LTRIGGER)
			if (audioVol > 0)
				audioVol -= 0x0800; // 16 steps on the audio

		if (pad.Buttons & PSP_CTRL_RTRIGGER)
			if (audioVol < 0x8000)
				audioVol += 0x0800; // 16 steps on the audio

		sceKernelDelayThread(100*1000);
	}

	if (OGG_eos == 1)
		printf("\n\n End of file\n");
	else
		printf("\n\n Playback stopped\n");

	runningFlag = 0; // kill the threads
	sceKernelSignalSema(bufferEmpty, 1);
	sceKernelSignalSema(bufferFull, 1);
	sceKernelDelayThread(1*1000*1000);

	//Release channel:
	sceAudioChRelease(OGG_audio_channel);

	goto mloop;

	return 0; // never reaches here - just supresses compiler warning
}
