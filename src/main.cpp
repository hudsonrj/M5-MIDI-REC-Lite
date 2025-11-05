#include <M5Atom.h>
#include <BLEMidi.h>
#include "driver/adc.h"
#include "BluetoothA2DPSource.h"
#include "driver/i2s.h"

#define NOTE_MAX 4000
#define REC_PLAY_LATENCY 65000

#define SM_REC_STANDBY 0
#define SM_PLAY 1
#define SM_REC 2
#define SM_PLAY_WAIT 3
#define SM_REC_WAIT 4
#define SM_PLAY_STANDBY 5
#define SM_MIC_MODE 6
#define SM_NOTREADY 9

#define CONFIG_I2S_BCK_PIN 19
#define CONFIG_I2S_LRCK_PIN 33
#define CONFIG_I2S_DATA_PIN 22
#define CONFIG_I2S_DATA_IN_PIN 23
#define SPAKER_I2S_NUMBER I2S_NUM_0

bool is_active = false;
int s_mode = SM_NOTREADY;
unsigned long st = 0;
unsigned long ct = 0;
unsigned long notes_time[NOTE_MAX];
short notes[NOTE_MAX][4]; // 0: Method, 1: CH, 2: var1, 3: var2
/*
[Method]
 0: note on, 1: note off, 3: CC
 4: PC, 5: PB, 6: AT, 7: ATP
 */
int cur_note = 0;

BluetoothA2DPSource a2dp_source;

CRGB dispColor(uint8_t r, uint8_t g, uint8_t b)
{
  return (CRGB)((r << 16) | (g << 8) | b);
}

void setTime()
{
  ct = (micros() - st) / 1000;
}

void play_notes()
{
  cur_note = 0;
  st = micros();
  s_mode = SM_PLAY;
}

void stop_notes()
{
  if (s_mode == SM_REC)
  {
    notes_time[cur_note] = 0;
  }
}

void rec_start()
{
  cur_note = 0;
  st = micros();
  s_mode = SM_REC;
}

unsigned long get_current_time()
{
  return (micros() - st);
}

// I2S configuration
i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// I2S pin configuration
i2s_pin_config_t pin_config = {
    .bck_io_num = CONFIG_I2S_BCK_PIN,
    .ws_io_num = CONFIG_I2S_LRCK_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = CONFIG_I2S_DATA_IN_PIN};

// audio data callback
int32_t get_audio_data(uint8_t *data, int32_t len) {
    size_t bytes_read;
    i2s_read(SPAKER_I2S_NUMBER, data, len, &bytes_read, portMAX_DELAY);
    return bytes_read;
}


void event_btn()
{
  if (s_mode == SM_REC_STANDBY)
  {
    s_mode = SM_REC_WAIT;
    M5.dis.drawpix(0, dispColor(238, 120, 0));
  }
  else if (s_mode == SM_PLAY)
  {
    stop_notes();
    s_mode = SM_REC_STANDBY;
    M5.dis.drawpix(0, dispColor(0, 0, 225));
  }
  else if (s_mode == SM_REC)
  {
    stop_notes();
    s_mode = SM_PLAY_STANDBY;
    M5.dis.drawpix(0, dispColor(225, 225, 225));
  }
  else if (s_mode == SM_PLAY_WAIT)
  {
    s_mode = SM_PLAY_STANDBY;
    M5.dis.drawpix(0, dispColor(225, 225, 225));
  }
  else if (s_mode == SM_REC_WAIT)
  {
    s_mode = SM_REC_STANDBY;
    M5.dis.drawpix(0, dispColor(0, 0, 255));
  }
  else if (s_mode == SM_PLAY_STANDBY)
  {
    s_mode = SM_PLAY_WAIT;
    M5.dis.drawpix(0, dispColor(255, 255, 0));
  }
}

void onConnected()
{
  // Serial.println("Bluetooth Connected\n");
  s_mode = SM_REC_STANDBY;
  M5.dis.drawpix(0, dispColor(0, 0, 255));
}
void onDisconnected()
{
  // Serial.println("Bluetooth Disconnected\n");
  s_mode = SM_NOTREADY;
  M5.dis.drawpix(0, dispColor(255, 0, 255));
}

void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
  if (s_mode == SM_REC)
  {
    notes_time[cur_note] = get_current_time();
    notes[cur_note][0] = 0;
    notes[cur_note][1] = (int)channel;
    notes[cur_note][2] = (int)note;
    notes[cur_note][3] = (int)velocity;
    cur_note++;
  }
  else if (s_mode == SM_PLAY_WAIT)
  {
    if (note == 0 && velocity < 5)
    {
      play_notes();
      M5.dis.drawpix(0, dispColor(0, 255, 0));
    }
  }
  else if (s_mode == SM_REC_WAIT)
  {
    if (note == 0 && velocity < 5)
    {
      rec_start();
      M5.dis.drawpix(0, dispColor(255, 0, 0));
    }
  }
  if (s_mode != SM_PLAY)
  {
    // Serial.println("NI CH:%d, NOTE:%d, V:%d\n", (channel + 1), note, velocity);
  }
}

void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
  if (s_mode == SM_REC)
  {
    notes_time[cur_note] = get_current_time();
    notes[cur_note][0] = 1;
    notes[cur_note][1] = (int)channel;
    notes[cur_note][2] = (int)note;
    notes[cur_note][3] = (int)velocity;
    cur_note++;
  }
  if (s_mode != SM_PLAY)
  {
    // Serial.println("NO CH:%d, NOTE:%d, V:%d\n", (channel + 1), note, velocity);
  }
}

void setup()
{
  // 勝手にボタン押下回避
  adc_power_acquire();

  M5.begin(true, false, true);

  M5.dis.drawpix(0, dispColor(0, 0, 0));

  // I2S setup
  i2s_driver_install(SPAKER_I2S_NUMBER, &i2s_config, 0, NULL);
  i2s_set_pin(SPAKER_I2S_NUMBER, &pin_config);
  i2s_set_clk(SPAKER_I2S_NUMBER, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  BLEMidiServer.begin("M5 MIDI REC Lite");
  BLEMidiServer.setOnConnectCallback(onConnected);
  BLEMidiServer.setOnDisconnectCallback(onDisconnected);
  BLEMidiServer.setNoteOnCallback(onNoteOn);
  BLEMidiServer.setNoteOffCallback(onNoteOff);

  M5.dis.drawpix(0, dispColor(255, 0, 255));
}

void loop()
{
  M5.update();
  if (M5.Btn.wasPressed())
  {
    event_btn();
  }
  if (M5.Btn.wasReleasefor(1000))
  {
      if (s_mode != SM_MIC_MODE) {
        s_mode = SM_MIC_MODE;
        M5.dis.drawpix(0, dispColor(0, 255, 255));
        a2dp_source.start("AtomEchoMic", get_audio_data);
        BLEMidiServer.stop();
      } else {
        s_mode = SM_REC_STANDBY;
        M5.dis.drawpix(0, dispColor(0, 0, 255));
        a2dp_source.stop();
        BLEMidiServer.begin("M5 MIDI REC Lite");
      }
  }
  if (s_mode == SM_PLAY)
  { // play
    setTime();
    for (int i; i < 20; i++)
    {
      if (notes_time[cur_note] == 0)
      {
        stop_notes();
        s_mode = SM_REC_STANDBY;
        M5.dis.drawpix(0, dispColor(0, 0, 255));
        break;
      }
      if (notes_time[cur_note] <= get_current_time() + REC_PLAY_LATENCY)
      {
        if (notes[cur_note][0] == 0)
        {
          BLEMidiServer.noteOn(0, notes[cur_note][2], notes[cur_note][3]);
        }
        else if (notes[cur_note][0] == 1)
        {
          BLEMidiServer.noteOff(0, notes[cur_note][2], notes[cur_note][3]);
        }
        cur_note++;
      }
      else
      {
        break;
      }
    }
  }
  else if (s_mode == SM_REC)
  { // rec
    setTime();
  }
}