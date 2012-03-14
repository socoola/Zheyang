#ifndef _C_AUDIO_H
#define _C_AUDIO_H

#define AUDIO_CHANNELS							1
#define AUDIO_SAMPLES_PER_SEC					8000

#define AUDIO_PCM_BITS_PER_SAMPLE				16
#define AUDIO_PCM_BLOCK_ALIGN					2

#define AUDIO_ADPCM_BITS_PER_SAMPLE				4
#define AUDIO_ADPCM_BLOCK_ALIGN					164
#define AUDIO_ADPCM_SAMPLES_PER_BLOCK			320

typedef void (* GET_AUDIO_CHUNK_CALLBACK)(AUDIO_CHUNK * chunk);

bool c_request_audio(GET_AUDIO_CHUNK_CALLBACK callback, AUDIO_CODEC codec);

bool c_end_audio(GET_AUDIO_CHUNK_CALLBACK callback);

bool c_request_speak(unsigned int buffertick);

bool c_speak(AUDIO_CHUNK * chunk);

bool c_end_speak();

bool c_init_audio();

bool c_do_audio_handler();

void c_deinit_audio();

#define MAX_SD_SENSITIVITY						9

void c_start_sound_detect(int sensitivity);

void c_stop_sound_detect();

bool c_is_sound_detected();

void c_reset_sound_detected();

#endif
