#include <stdio.h>
#include <memory.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <xdo.h>

#define SAMPLE_RATE 44100

typedef struct recordState {
  int count;
  /* 0 = Normal, 1 = Spike low, 2 = Spike high, 3 = Noise / ACTIVE, 4 = Spike high, 5 = Spike low  */
  int state;

  xdo_t* xdo;
} RecordState;

#define HIGH_THRESHOLD 255
#define LOW_THRESHOLD 1

#define STATE_1_LEN 0.15 * SAMPLE_RATE * 0.8
#define STATE_2_LEN 0.75 * SAMPLE_RATE * 0.1
#define STATE_3_LEN 0.1 * SAMPLE_RATE * 0.8
#define STATE_4_LEN 0.01 * SAMPLE_RATE * 0.8
#define STATE_5_LEN 1 * SAMPLE_RATE * 0.8

void update(RecordState* userData) {
  /* Stupid and naïve but whatever */
  int oldCount = userData->count++;
  if(oldCount > userData->count) {
    userData->count = oldCount;
  }
}

void readAudio(RecordState* userData, Uint8 *stream, int len) {
  /* printf("Read audio! %d\n", stream[0]); */
  for(int i = 0; i < len; ++i) {
    Uint8 sample = stream[i];

    int oldState = userData->state;

    if(userData->state == 0) {
      if(sample <= LOW_THRESHOLD) {
	update(userData);
	if(userData->count >= STATE_1_LEN) {
	  userData->count = 0;
	  userData->state = 1; // Low spike
	}
      } else {
	userData->count = 0;
      }
    } else if(userData->state == 1) {
      if(sample >= HIGH_THRESHOLD) {
	update(userData);
	if(userData->count >= STATE_2_LEN) {
	  userData->count = 0;
	  userData->state = 2; // High spike
	}
      } else {
	userData->count = 0;
      }
    } else if(userData->state == 2) {
      if(sample < HIGH_THRESHOLD) {
	update(userData);
	if(userData->count >= STATE_3_LEN) {
	  userData->count = 0;
	  userData->state = 3; // Active
	  printf("Going into active state!\n");
	  xdo_send_keysequence_window(userData->xdo, CURRENTWINDOW, "XF86AudioPlay", 0);
	}
      } else {
	userData->count = 0;
      }
    } else if(userData->state == 3) {
      if(sample >= HIGH_THRESHOLD) {
	update(userData);
	if(userData->count >= STATE_4_LEN) {
	  userData->count = 0;
	  userData->state = 4; // High spike
	}
      } else if(sample <= LOW_THRESHOLD) {
	userData->state = 5; // Low spike in case of tiny boyes
      }
    } else if(userData->state == 4) {
      if(sample <= LOW_THRESHOLD) {
	update(userData);
	if(userData->count >= STATE_5_LEN) {
	  userData->count = 0;
	  userData->state = 5; // Low spike
	}
      } else {
	userData->count = 0;
      }
    } else if(userData->state == 5) {
      if(sample > LOW_THRESHOLD) {
	userData->state = 0; // Normal
	userData->count = 0;
	/* xdo_send_keysequence_window_up(userData->xdo, CURRENTWINDOW, "XF86AudioPlay", 0); */
	printf("Returning to normal state!\n");
      }
    }
    if(oldState != userData->state) {
      printf("New State! %d\n", userData->state);
    }
  }
}

void main() {
  if(SDL_Init(SDL_INIT_AUDIO) != 0) {
    printf("Err.. %s", SDL_GetError());
  }
  
  SDL_AudioSpec spec;
  spec.freq = SAMPLE_RATE;
  spec.format = AUDIO_U8;
  spec.channels = 1;
  spec.samples = 16384;
  spec.callback = (*readAudio);

  RecordState state;
  state.state = 0;
  state.count = 0;
  state.xdo = xdo_new(NULL);
  spec.userdata = &state;

  int deviceCount = SDL_GetNumAudioDevices(0);
  const char* deviceName;
  for(int i = 0; i < deviceCount; ++i) {
    deviceName = SDL_GetAudioDeviceName(i, 0);
    printf("Found device %s\n", deviceName);
  }

  int dev = SDL_OpenAudioDevice(deviceName, 1, &spec, NULL, 0);
  if(!dev) {
    printf("Error opening %s! %s", deviceName, SDL_GetError());
    return;
  }
  printf("Listening for pulses!");

  SDL_PauseAudioDevice(dev, 0);

  while(1) {
    SDL_Delay(0);
  }
}
