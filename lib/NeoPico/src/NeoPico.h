#ifndef _NEO_PICO_H_
#define _NEO_PICO_H_

#include "ws2812.pio.h"
#include <vector>

typedef enum
{
  LED_FORMAT_GRB = 0,
  LED_FORMAT_RGB = 1,
  LED_FORMAT_GRBW = 2,
  LED_FORMAT_RGBW = 3,
} LEDFormat;

#define FRAME_MAX 100

class NeoPico
{
public:
  NeoPico();
  void Setup(int ledPin, int inNumPixels, LEDFormat inFormat, PIO inPio, int inState);
  void Show();
  void Clear();
  void Off();
  LEDFormat GetFormat();
  void SetFrame(uint32_t * newFrame);
  void ChangeNumPixels(int inNumPixels) {numPixels = inNumPixels;}
private:
  void PutPixel(uint32_t pixel_grb);
  LEDFormat format;
  PIO pio = pio1;
  int stateMachine = 0;
  int numPixels = 0;
  uint32_t frame[FRAME_MAX];
};

#endif
