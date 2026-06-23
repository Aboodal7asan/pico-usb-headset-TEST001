#include "pico/stdlib.h"
#include "pico/machine_i2s.h"
#include "main.h"
#include "usb_microphone.h"

// --- HARDCODED PINS AND SETTINGS ---
#define I2S_MIC_SCK 18
#define I2S_MIC_WS 19
#define I2S_MIC_SD 20
#define I2S_MIC_BPS 32
#define I2S_MIC_RATE_DEF 48000
#define SIZEOF_DMA_BUFFER_IN_BYTES 256
#define STEREO 0
#define RX 1

//-------------------------
// Onboard LED
//-------------------------
const uint LED_PIN = 25;
//-------------------------

//-------------------------
// variables
//-------------------------
usb_audio_sample sample_buffer[USB_MIC_SAMPLE_BUFFER_SIZE];
//-------------------------

//-------------------------
// callback functions
//-------------------------
void on_usb_microphone_tx_ready();
void on_usb_microphone_tx_done();
//-------------------------

// Pointer to I2S handler
machine_i2s_obj_t* i2s0 = NULL;

usb_audio_sample mic_i2s_to_usb_sample_convert(uint32_t sample_idx, uint32_t sample);

int main(void)
{
  stdio_init_all();

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  i2s0 = create_machine_i2s(0, I2S_MIC_SCK, I2S_MIC_WS, I2S_MIC_SD, RX, I2S_MIC_BPS, STEREO, SIZEOF_DMA_BUFFER_IN_BYTES, I2S_MIC_RATE_DEF);

  // initialize the USB microphone interface
  usb_microphone_init();
  usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);
  usb_microphone_set_tx_done_handler(on_usb_microphone_tx_done);

  while (1) {
    // run the USB microphone task continuously
    usb_microphone_task();
  }

  return 0;
}

void on_usb_microphone_tx_ready()
{
  gpio_put(LED_PIN, 1);
  usb_microphone_write(&sample_buffer, sizeof(sample_buffer));
  gpio_put(LED_PIN, 0);
}

void on_usb_microphone_tx_done()
{
  i2s_32b_audio_sample buffer[USB_MIC_SAMPLE_BUFFER_SIZE];
  if(i2s0) {
    int num_bytes_read = machine_i2s_read_stream(i2s0, (void*)&buffer[0], sizeof(buffer));

    if(num_bytes_read >= I2S_RX_FRAME_SIZE_IN_BYTES) {
      int num_of_frames_read = num_bytes_read/I2S_RX_FRAME_SIZE_IN_BYTES;
      for(uint32_t i = 0; i < num_of_frames_read; i++){
          sample_buffer[i] = mic_i2s_to_usb_sample_convert(i, buffer[i].left); 
      }
    }
  }
}

// --- THE BASS FILTER (DC BLOCKER) ---
static float dc_offset = 0.0f;

usb_audio_sample mic_i2s_to_usb_sample_convert(uint32_t sample_idx, uint32_t sample)
{
  int32_t sample_tmp = (int32_t)sample;

  // Leaky integrator to strip out the muddy bass and DC offset
  dc_offset = (0.99f * dc_offset) + (0.01f * (float)sample_tmp);
  sample_tmp = (int32_t)((float)sample_tmp - dc_offset);

  return sample_tmp; 
}
