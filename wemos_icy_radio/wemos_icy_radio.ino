#include <ESP8266WiFi.h>
//#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
#include "secrets.h"

AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceICYStream *file = NULL;
AudioFileSourceBuffer *buff = NULL;
AudioOutputI2SNoDAC *out = NULL;
/*
const int preallocateBufferSize = 2048;
void *preallocateBuffer = NULL;
*/
//const char* URL = "https://www.soundhelix.com/examples/mp3/SoundHelix-Song-6.mp3";
//const char* URL = "http://hearme.fm:8518/stream"; // 128kbps mp3 DEMO INTERNET RADIO doesn't work.. will try const char*
//const char* URL = "http://jazz.streamr.ru/jazz-64.mp3";
// I think 128kbps but 1channel works fine...
//const char* URL = "http://hd.lagrosseradio.info:8000/lagrosseradio-rock-064.mp3"; // 1 channel 44100hz
// 2 CHANNELS BELOW...
//const char* URL = "http://c6.radioboss.fm:8127/autodj"; // 2 channels 44100hz
//const char* URL = "https://stream.darkedge.ro:8002/stream/1/";
const char* URL = "http://play.global.audio/radio1rock64";
bool audioFlag = false;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  // Set up radio
  file = new AudioFileSourceICYStream(URL);
  buff = new AudioFileSourceBuffer(file, 8192);
  out = new AudioOutputI2SNoDAC();
  mp3 = new AudioGeneratorMP3();
  mp3->begin(buff, out);

  audioFlag = true;
}

void loop() {
  // put your main code here, to run repeatedly:
  // Serial.println("Hello");
  //static int lastms = 0;
  if(audioFlag){
    if (mp3->isRunning()) {
      /*
      if (millis()-lastms > 1000) {
        lastms = millis();
        Serial.printf("Running for %d ms...\n", lastms);
        //Serial.flush();
      }
      */
    if (!mp3->loop()) mp3->stop();
    } else {
      Serial.println("Stopping radio");
      audioFlag = false;
    }
  }
}
