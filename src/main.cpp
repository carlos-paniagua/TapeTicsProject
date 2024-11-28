#include <M5Atom.h>

#define USE_BLE
// #define USE_OSC
// #define USE_OTHER_FUNCTIONS
// #define USE_SERIAL_COMMAND

int preset_time = 10000;
int steps = 0;

void ledOnSingle(int i);
void ledOn();
String getSerialCommand();

void handleBLECommand(std::string value,unsigned long currentTime);
void addMessageToQueue(String command);
void processQueueTask(void *pvParameters);

void setRGB(String s,unsigned long currenttime);
void setAllRGB(String s);
void printRGB();

// 追加関数
#ifdef USE_OTHER_FUNCTIONS
void initializeHBM();
void SoundMode();
void shake();
void IRsensor();
void MuscleMode();
void HeartBeatMode();
void onBeatDetected();
#endif

//アニメーション
void waveAnimation();
void randomAnimation();
void shockAnimation();

// メッセージキューの定義
////////////////////////////////////////////////////////////////////
#include <cppQueue.h>

cppQueue messageQueue(sizeof(String), 1000, FIFO, false); // FIFOキューの初期化
bool isProcessing;
unsigned long currentTaskEndTime;
unsigned long delayval; // 適切なdelay値を設定してください
bool isAnimating;

// LED
///////////////////////////////////////////////////////////////////////
#include <FastLED.h>
#include <Wire.h>

#define PIN 22
#define NUMMAX 60
#define NUMSETS 11
#define NUMPIXELS (NUMMAX * 2)
#define LED_TYPE WS2812
#define COLOR_ORDER RGB
#define BRIGHTNESS 255

#define LED_COLOR_GREEN (0x001B00)  // LEDのカラーコード 緑
#define LED_COLOR_RED (0xff0000)    // LEDのカラーコード 赤
#define LED_COLOR_YELLOW (0xffff00) // LEDのカラーコード 赤
#define LED_COLOR_OFF (0x000000)    // LEDのカラーコード 黒

CRGB leds[NUMPIXELS];
int power[NUMMAX];                            // セットごとのパワーの配列
int red[NUMMAX];                              // セットごとの赤の配列
int green[NUMMAX];                            // セットごとの緑の配列
int blue[NUMMAX];                             // セットごとの青の配列
unsigned long lightingDuration[NUMMAX] = {0}; // セットごとの点灯時間（秒）
unsigned long nodeEndTime[NUMMAX] = {0};      // 各ノードの終了時間
bool nodeProcessing[NUMMAX] = {false};        // 各ノードの処理状態

bool startExecution = false; // ボタンが押されたら true になる

CRGB getPixelColor1(uint8_t power)
{
  return CRGB(power, 0, 0);
}

CRGB getPixelColor2(uint8_t red, uint8_t green, uint8_t blue)
{
  return CRGB(green, red, blue);
}

// BLE
///////////////////////////////////////////////////////////////////////
#ifdef USE_BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

String SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0";        // サービスUUIDのデフォルト値
String CHARACTERISTIC_UUID = "abcdef12-3456-7890-abcd-ef1234567890"; // キャラクタリスティックUUIDのデフォルト値
String BLE_DEVICE_NAME = "Tapetics_Controller";                      // BLEデバイス名のデフォルト値

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override
  {
    Serial.println("Client connected");
    deviceConnected = true;
    if (deviceConnected)
    {
      M5.dis.fillpix(LED_COLOR_GREEN);
    }
  }
  void onDisconnect(BLEServer *pServer) override
  {
    Serial.println("Client disconnected");
    deviceConnected = false;
    BLEDevice::startAdvertising();
    M5.dis.fillpix(LED_COLOR_RED);
  }
};

void sendDataOverBLE(const char *data)
{
  if (deviceConnected && pCharacteristic != NULL)
  {
    pCharacteristic->setValue(data);
    pCharacteristic->notify();
  }
}

void initializeBLE()
{
  BLEDevice::init(BLE_DEVICE_NAME.c_str());
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID.c_str());

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID.c_str(),
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_INDICATE);
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID.c_str());
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE initialize complete");

  M5.dis.fillpix(LED_COLOR_RED);
}

#endif // USE_BLE

// OSC
///////////////////////////////////////////////////////////////////////
#ifdef USE_OSC
#include <ArduinoOSCWiFi.h>

const char* ssid = "Buffalo-G-6368"; // WiFi SSIDを設定
const char* password = "5sxhmntn7nr7y"; // WiFi パスワードを設定
const IPAddress ip(192, 168, 11, 43); // デバイスの固定IPアドレス
const IPAddress gateway(192, 168, 11, 1); // ゲートウェイのIPアドレス
const IPAddress subnet(255, 255, 255, 0); // サブネットマスク

const char* host = "192.168.11.3"; // 送信先のホストIPアドレス
const int recv_port = 8001; // 受信ポート番号   
const int send_port = 9001; // 送信ポート番号

void sendDataOverOSC(const char* address, const char* data)
{
  OSCMessage msg(address);
  msg.add(data);
  udp.beginPacket(outIp, send_port);
  msg.send(udp);
  udp.endPacket();
  msg.empty();
}

void initializeOSC()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("WiFi connected, IP = ");
  Serial.println(WiFi.localIP());

  udp.begin(inPort); 
  Serial.println("OSC initialized and listening");
}
#endif // USE_OSC

//メイン処理
///////////////////////////////////////////////////////////////////////////
void setup()
{
  M5.begin(true, false, true); // 本体初期化（UART, I2C, LED）
  M5.IMU.Init();               // IMUを使うとき解除

  FastLED.addLeds<LED_TYPE, PIN, COLOR_ORDER>(leds, NUMMAX).setCorrection(TypicalLEDStrip);
  FastLED.setDither(DISABLE_DITHER);

#ifdef USE_BLE
  initializeBLE();
#endif

#ifdef USE_OSC
  initializeOSC();
#endif

  // キュー処理タスクを起動
  xTaskCreatePinnedToCore(processQueueTask, "QueueTask", 4096, NULL, 1, NULL, 0);

  Serial.println("Initialize complete");
}

void loop()
{
  M5.update();
  unsigned long currentTime = millis();

  // BLEからの文字列
#ifdef USE_BLE
  if (deviceConnected) {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      handleBLECommand(value,currentTime);
      pCharacteristic->setValue("");
    }
  }
#endif

#ifdef USE_SERIAL_COMMAND
  if (Serial.available() > 0)
  {
    String s = getSerialCommand();
    if (s.length() > 0) // 空のメッセージを無視
    {
      if (s == "run")
      {
        if (!startExecution) // 初回のみ出力
        {
          startExecution = true;
          // Serial.println("Starting execution of queued messages.");
        }
      }
      else if (s == "wave" || s == "shock" || s == "random")
      {
        // Call setRGB when one of these commands is received
        setRGB(s.c_str(),0);
      }
      else
      {
        String *message = new String(s);
        if (!messageQueue.isFull())
        {
          // messageQueue.push(&message); // メッセージをキューに追加
          // Serial.print("Queue count: ");
          // Serial.println(messageQueue.getCount());
        }
      }
    }
  }
#endif

  // M5Stackのボタンを押したら実行開始
  if (M5.Btn.isPressed())
  {
    startExecution = true;
    Serial.println("Starting execution of queued messages.");
  }
  
  
  if (startExecution)
  {
    for (int steps = 0; steps < NUMSETS; steps++)
    {
      if (nodeProcessing[steps] && currentTime  >= nodeEndTime[steps])
      {
        nodeProcessing[steps] = false;
        nodeEndTime[steps] = 0;
      }
    }
  }

  delay(5);
}

void handleBLECommand(std::string value,unsigned long currentTime) {
  String command = String(value.c_str());
  if (command == "run") {
    startExecution = true;
    Serial.println("Starting execution of queued messages.");
  } else if (command == "endSend") {
    Serial.print("Queue count after endSend: ");
    Serial.println(messageQueue.getCount());
  } else if (command == "wave" || command == "shock" || command == "random") {
    setRGB(command, currentTime);
  } else {
    addMessageToQueue(command);
  }
}

void addMessageToQueue(String command) {
  if (!messageQueue.isFull())
  {
    String strValue = String(command.c_str());
    int startIndex = 0;
    int endIndex = strValue.indexOf('\n');

    while (endIndex != -1)
    {
      String line = strValue.substring(startIndex, endIndex);

      String *msg = new String(line);
      messageQueue.push(&msg);
      // Serial.print(messageQueue.getCount());
      // Serial.print("  Queued message: ");
      // Serial.println(line);

      startIndex = endIndex + 1;
      endIndex = strValue.indexOf('\n', startIndex);
    }
  }
  else {
    Serial.println("Warning: Queue is full! Unable to queue more messages.");
  }
}

// void processQueueTask(void *pvParameters)
// {
//   while (true) {
//     String *msg;
//     if (startExecution && messageQueue.peek(&msg)) {
//       // Directly pop and retrieve the message
//       if (messageQueue.pop(&msg)) {
//         if (msg) {
//           // Serial.println(messageQueue.getCount());
//           // Serial.print(" : ");
//           // Serial.println(*msg);
//           setRGB(*msg, millis());  // Call your function to handle the RGB setting
//           delete msg;    // Ensure proper memory cleanup
//         }
//       } 
//       else {
//         Serial.println("Failed to pop from the queue.");
//         vTaskDelay(10 / portTICK_PERIOD_MS); 
//       }
//     }
//     else {
//       vTaskDelay(10 / portTICK_PERIOD_MS); // Longer delay when idle
//     }
//     // Check if all messages have been processed
//     if (messageQueue.isEmpty() && startExecution) {
//       setAllRGB("0,0,0,0,0");  // Reset or finish RGB task
//       Serial.println("All messages processed. FINISH");
//       startExecution = false;
//     }
//     delay(10);
//   }
// }

void processQueueTask(void *pvParameters)
{
  String lastMessage = "";
  while (true)
  {
    if (startExecution) // ボタンが押されたらキュー処理を開始
    {
      String *msg;
      if (messageQueue.peek(&msg))
      {
        bool validFormat = true;
        int setIndex, pwr, r, g, b;
        float time;

        if (*msg == lastMessage)
        {
          delete msg;
          messageQueue.pop(&msg);
          continue;
        }

        // メッセージをカンマで分割
        int commaIndex = 0;
        String parameters[6]; // 6つのパラメータを期待
        String remaining = *msg;

        for (int i = 0; i < 6; i++) {
          commaIndex = remaining.indexOf(',');
          if (commaIndex == -1 && i < 5) {
            validFormat = false;
            break;
          }

          if (i == 5) {
            parameters[i] = remaining;
          } else {
            parameters[i] = remaining.substring(0, commaIndex);
            remaining = remaining.substring(commaIndex + 1);
          }
        }

        // 6個のパラメータが揃っているか確認
        if (validFormat) {
          setIndex = parameters[0].toInt();
          pwr = parameters[1].toInt();
          r = parameters[2].toInt();
          g = parameters[3].toInt();
          b = parameters[4].toInt();
          time = parameters[5].toFloat();
        } else {
          // フォーマットが無効な場合は通常のコマンドとして扱う
          String command = *msg;
          if (messageQueue.pop(&msg))  // キューからメッセージをポップ
          {
            // Serial.print(messageQueue.getCount());
            // Serial.print("  pop from queue :");
            // Serial.println(command);

            setRGB(command, millis());  // コマンド処理関数の呼び出し
            delete msg;  // メモリ解放
          }
          continue;
        }

        // 有効なフォーマットの場合はさらに処理
        setIndex -= 1;
        if (!nodeProcessing[setIndex] && nodeEndTime[setIndex] == 0)
        {
          if (messageQueue.pop(&msg))  // キューからメッセージをポップ
          {
            // Serial.print(messageQueue.getCount());
            // Serial.print("  pop from queue :");
            // Serial.println(*msg);

            setRGB(*msg, millis());  // コマンド処理関数の呼び出し
            delete msg;  // メモリ解放
          }
        }
      }
      else
      {
        vTaskDelay(5 / portTICK_PERIOD_MS); // キューが空の場合、再試行待機
      }
    }
    else
    {
      vTaskDelay(5 / portTICK_PERIOD_MS); // ボタンが押されるのを待機
    }

    // すべてのメッセージが処理されているか確認
    if (messageQueue.getCount() == 0 && startExecution)
    {
      setAllRGB("0,0,0,0,0");  // RGBをリセット
      Serial.println("All messages processed. FINISH");
      startExecution = false;
    }
    delay(5); // 少し待機してループを繰り返す
  }
}

void ledOnSingle(int i)
{
  leds[i * 2] = getPixelColor1(power[i]);
  leds[i * 2 + 1] = getPixelColor2(red[i], green[i], blue[i]);

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();
}

void ledOn()
{
  for (int i = 0; i < NUMSETS; i++)
  {
    leds[i * 2] = getPixelColor1(power[i]);
    leds[i * 2 + 1] = getPixelColor2(red[i], green[i], blue[i]);
  }

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();
}

#ifdef USE_SERIAL_COMMAND
String getSerialCommand()
{
  String s = "";
  while (Serial.available() > 0)
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      break; // 改行が来たら終了
    }
    s += c;
  }
  s.trim();
  return s;
}
#endif

// void setRGB(String s, int currenttime)
void setRGB(String s, unsigned long currentTime)
{
  if (s.startsWith("all"))
  {
    setAllRGB(s.substring(4));
  }
  else if (s.startsWith("wave"))
  {
    Serial.println("Wave Mode");
    waveAnimation();
  }
  else if (s.startsWith("shock"))
  {
    Serial.println("Shock Mode");
    shockAnimation();
  }
  else if (s.startsWith("random"))
  {
    Serial.println("Random Mode");
    randomAnimation();
  }
  else
  {
    // 各値をカンマ区切りでパース
    int setIndex, pwr, r, g, b;
    float time;

    int startIdx = 0;
    int commaIdx = s.indexOf(',');

    // 1. setIndex
    setIndex = s.substring(startIdx, commaIdx).toInt() - 1;
    startIdx = commaIdx + 1;
    commaIdx = s.indexOf(',', startIdx);

    // 2. pwr
    pwr = s.substring(startIdx, commaIdx).toInt();
    startIdx = commaIdx + 1;
    commaIdx = s.indexOf(',', startIdx);

    // 3. r
    r = s.substring(startIdx, commaIdx).toInt();
    startIdx = commaIdx + 1;
    commaIdx = s.indexOf(',', startIdx);

    // 4. g
    g = s.substring(startIdx, commaIdx).toInt();
    startIdx = commaIdx + 1;
    commaIdx = s.indexOf(',', startIdx);

    // 5. b
    b = s.substring(startIdx, commaIdx).toInt();
    startIdx = commaIdx + 1;

    // 6. time
    time = s.substring(startIdx).toFloat();
    // time = 0.05;

    if (setIndex >= 0 && setIndex < NUMSETS)
    {
      power[setIndex] = pwr;
      red[setIndex] = r;
      green[setIndex] = g;
      blue[setIndex] = b;
      nodeProcessing[setIndex] = true;

      if (time > 0)
      {
        nodeProcessing[setIndex] = true;
        nodeEndTime[setIndex] =  millis() + (unsigned long)(time * 1000);
      }
      else
      {
        power[setIndex] = 0;
        red[setIndex] = 0;
        green[setIndex] = 0;
        blue[setIndex] = 0;
        nodeProcessing[setIndex] = false;
        nodeEndTime[setIndex] = 0;
      }

      ledOnSingle(setIndex);
    }

    // if (setIndex >= 0 && setIndex < NUMSETS) {
    //   power[setIndex] = pwr;
    //   red[setIndex] = r;
    //   green[setIndex] = g;
    //   blue[setIndex] = b;

    //   nodeProcessing[setIndex] = true;
    //   nodeEndTime[setIndex] = millis() + (unsigned long)(time * 1000);
    //   ledOnSingle(setIndex);
    // }
  }
}

void setAllRGB(String s)
{
  int pwr, r, g, b, time;
  sscanf(s.c_str(), "%d,%d,%d,%d,%d", &pwr, &r, &g, &b, &time);
  for (int i = 0; i < NUMSETS; i++)
  {
    power[i] = pwr;
    red[i] = r;
    green[i] = g;
    blue[i] = b;
    lightingDuration[i] = time;
  }

  ledOn(); // ここにledonを入れないと起動しない
}

void printRGB()
{
  Serial.println("--------------------------------------");
  for (int i = 0; i < NUMSETS; i++)
  {
    Serial.printf("(Set %d, Power, R, G, B, Duration) = (%d, %d, %d, %d, %d)\n", i + 1, power[i], red[i], green[i], blue[i], lightingDuration[i]);
  }
}

#ifdef USE_OTHER_FUNCTIONS
// 心拍
///////////////////////////////////////////////////////////////////////////
// PulseOximeter pox;

// void initializeHBM()
// {
//   Serial.println("Initializing pulse oximeter..");
//   if (!pox.begin())
//   {
//     Serial.println("FAILED");
//     for (;;)
//       ;
//   }
//   else
//   {
//     Serial.println("SUCCESS");
//   }
//   pox.setOnBeatDetectedCallback(onBeatDetected);
// }

// 筋電位
///////////////////////////////////////////////////////////////////////////
// int EMG_PIN = 33;    // GPIO pin where MyoWare 2.0 sensor is connected
// int THRESHOLD = 500; // Threshold value to detect muscle contraction


// 追加関数
///////////////////////////////////////////////////////////////
// void HeartBeatMode()
// {
//   M5.dis.fillpix(LED_COLOR_YELLOW);
//   int Threshold = 3000; // ビートとしてカウントする閾値
//   int Signal = 0;
//   const int numReadings = 10;      // 移動平均フィルターのウィンドウサイズ
//   int readings[numReadings] = {0}; // 読み取り値を保存する配列
//   int readIndex = 0;               // 現在の配列のインデックス
//   int total = 0;                   // 読み取り値の合計
//   int average = 0;                 // 読み取り値の平均
//   bool pulseDetected = false;      // パルスが検出されたかどうか
//   unsigned long lastBeatTime = 0; // 前回の心拍時間
//   unsigned long beatInterval = 0; // 心拍間隔
//   int bpm = 0;                    // 心拍数
//   // 読み取り値配列を初期化
//   for (int thisReading = 0; thisReading < numReadings; thisReading++)
//   {
//     readings[thisReading] = 0;
//   }
//   String command = "";
//   while (command != "end")
//   {
//     if (Serial.available() > 0)
//     {
//       command = getSerialCommand();
//     }
//     Signal = analogRead(PulseSensorPurplePin);  // PulseSensorの値を読み取る
//     Serial.println("Signal " + String(Signal)); // シリアルプロッターに値を送る
//     if (Signal == 4095)
//     {
//       setAllRGB("0,0,0,0,0");
//     }
//     else if (Signal > Threshold)
//     {
//       setAllRGB("50,50,0,0,0");
//     }
//     else
//     {
//       setAllRGB("0,0,0,0,0");
//     }
//     delay(20); // 20ミリ秒の遅延
//   }
//   Serial.println("End HeartBeat Mode");
//   setAllRGB("all,0,0,0,0,0");
// }

// void HeartBeatMode()
// {
//   initializeHBM()
//   M5.dis.fillpix(LED_COLOR_YELLOW);
//   // uint8_t Heart_rate = 0;
//   // uint8_t Spo2 = 0;
//   uint32_t tsLastReport = 0;
//   String command = "";
//   while (command != "end")
//   {
//     if (Serial.available() > 0)
//     {
//       command = getSerialCommand();
//     }
//     pox.update();
//     if (millis() - tsLastReport > REPORTING_PERIOD_MS)
//     {
//       Serial.print("Heart rate:");
//       Serial.print(pox.getHeartRate());
//       Serial.print(" bpm / SpO2:");
//       Serial.print(pox.getSpO2());
//       Serial.println(" %");
//       if (heartRate > 0) // 心拍が検出された場合のみLEDを点灯
//       {
//         setAllRGB("50,255,0,0,1"); // 点灯時間を1秒に設定
//       }
//       else
//       {
//         setAllRGB("0,0,0,0,0"); // 心拍が検出されない場合は消灯
//       }
//       tsLastReport = millis();
//     }
//   }
//   Serial.println("End HeartBeat Mode");
//   setAllRGB("all,0,0,0,0,0");
// }

// void onBeatDetected()
// {
//   Serial.println("Beat detected!");
//   M5.dis.fillpix(LED_COLOR_YELLOW); // Turn the screen red to indicate a beat
//   delay(100);                       // Keep the screen red for a short time
//   M5.dis.fillpix(LED_COLOR_OFF);    // Turn the screen back to black
// }

// void SoundMode_old()
// {
//   M5.dis.fillpix(LED_COLOR_YELLOW);
//   int max_value = 50;  // Maximum value for red, green, blue, power
//   int value = 255;
//   int nodeval = value / NUMSETS;
//   int scaled_nodeval = max_value / NUMSETS;  // Scaled nodeval
//   while (true)
//   {
//     if (deviceConnected)
//     {
//       std::string command = pCharacteristic->getValue();
//       if (command == "end")
//       {
//         break;
//       }
//       else if (command.length() >= 2)
//       {
//         int amplitude = std::stoi(command); // 修正箇所
//         if (amplitude >= 0 && amplitude <= value)
//         {
//           int scaled_amplitude = (amplitude * max_value) / value;  // Scale amplitude to 0-50 range
//           int fullLeds = scaled_amplitude / scaled_nodeval;
//           int remainder = scaled_amplitude % scaled_nodeval;
//           // すべてのLEDを消灯
//           for (int i = 0; i < NUMSETS; i++)
//           {
//             power[i] = 0;
//             red[i] = 0;
//             green[i] = 0;
//             blue[i] = 0;
//           }
//           // LEDの割合を計算
//           int greenLeds = NUMSETS * 6 / 11;
//           int yellowLeds = NUMSETS * 3 / 11;
//           int redLeds = NUMSETS * 2 / 11;
//           for (int i = 0; i < fullLeds; i++)
//           {
//             if (i < greenLeds)
//             {
//               green[i] = max_value;
//             }
//             else if (i < greenLeds + yellowLeds)
//             {
//               red[i] = max_value;
//               green[i] = max_value;
//             }
//             else
//             {
//               red[i] = max_value;
//             }
//             power[i] = max_value;
//           }
//           // Set partially lit LEDs
//           if (fullLeds < NUMSETS)
//           {
//             if (fullLeds < greenLeds)
//             {
//               green[fullLeds] = remainder;
//             }
//             else if (fullLeds < greenLeds + yellowLeds)
//             {
//               red[fullLeds] = remainder;
//               green[fullLeds] = remainder;
//             }
//             else
//             {
//               red[fullLeds] = remainder;
//             }
//             power[fullLeds] = remainder;
//           }
//         }
//       }
//       ledOn();
//     }
//   }
//   Serial.println("End SoundMode");
//   setAllRGB("0,0,0,0,0");
// }

// void SoundMode()
// {
//   M5.dis.fillpix(LED_COLOR_YELLOW);
//   int max_value = 50; // Maximum value for red, green, blue, power
//   int value = 255;
//   int nodeval = value / NUMSETS;
//   int scaled_nodeval = max_value / NUMSETS; // Scaled nodeval

//   while (true)
//   {
//     if (deviceConnected)
//     {
//       std::string command = pCharacteristic->getValue();
//       if (command == "end")
//       {
//         break;
//       }
//       else if (command.length() >= 2)
//       {
//         int amplitude = std::stoi(command); // 修正箇所
//         if (amplitude >= 0 && amplitude <= value)
//         {
//           int scaled_amplitude = (amplitude * max_value) / value; // Scale amplitude to 0-50 range
//           int red = 0, green = 0, blue = 0;

//           if (amplitude <= 150)
//           {
//             green = scaled_amplitude; // Set green to max value 50
//           }
//           else if (amplitude <= 200)
//           {
//             red = scaled_amplitude;
//             green = scaled_amplitude; // Set yellow to max value 50
//           }
//           else
//           {
//             red = scaled_amplitude; // Set red to max value 50
//           }

//           String colorCommand = String(scaled_amplitude) + "," + String(red) + "," + String(green) + "," + String(blue) + "," + String(0);
//           setAllRGB(colorCommand);
//         }
//       }
//       // ledOn();
//     }
//   }
//   Serial.println("End SoundMode");
//   setAllRGB("0,0,0,0,0");
//   M5.dis.fillpix(LED_COLOR_GREEN);
// }

// void MuscleMode()
// {
//   pinMode(EMG_PIN, INPUT);
//   const int VOLT = 3.3;        // 3.3Vを電源とした場合
//   const int ANALOG_MAX = 4096; // ESP32の場合
//   while (true)
//   {
//     // if (deviceConnected)
//     // {
//     //   std::string command = pCharacteristic->getValue();
//     if (Serial.available() > 0)
//     {
//       String command = getSerialCommand();
//       if (command == "end")
//       {
//         break;
//       }
//     }

//     int emgValue = analogRead(EMG_PIN);
//     float voltage = ((long)emgValue * VOLT * 1000) / ANALOG_MAX;
//     // Serial.print("EMG Value: ");
//     Serial.println(voltage);

//     // if (emgValue > THRESHOLD)
//     // {
//     //   Serial.println("Muscle contraction detected.");
//     //   M5.dis.fillpix(LED_COLOR_YELLOW);
//     //   delay(1000);
//     //   M5.dis.fillpix(LED_COLOR_OFF);
//     // }
//     // else
//     // {
//     //   Serial.println("No muscle contraction.");
//     // }

//     // delay(100);
//   }
//   Serial.println("End MuscleMode");
//   setAllRGB("0,0,0,0,0");
//   M5.dis.fillpix(LED_COLOR_GREEN);
// }

// void shake()
// {
//   M5.dis.fillpix(LED_COLOR_OFF);
//   float thresholdimu = 2; // 調整可能

//   while (true)
//   {
//     // M5.update();
//     // if (deviceConnected)
//     // {
//     //   std::string command = pCharacteristic->getValue();
//     if (Serial.available() > 0)
//     {
//       String command = getSerialCommand();
//       if (command == "end")
//       {
//         break;
//       }
//     }
//     float accX, accY, accZ;

//     // 加速度の読み取り
//     M5.IMU.getAccelData(&accX, &accY, &accZ);

//     Serial.print(accX);
//     Serial.print(",");
//     Serial.print(accY);
//     Serial.print(",");
//     Serial.println(accZ);

//     // 閾値を超えた場合に振動
//     if (abs(accZ) > thresholdimu)
//     {
//       Serial.println("vibrate");
//       setAllRGB("50,50,0,0,0");
//       delay(1000);
//       setAllRGB("0,0,0,0,0");
//     }

//     unsigned long currentTime = millis();
//     for (int i = 0; i < NUMSETS; i++)
//     {
//       if (lightingDuration[i] > 0 && currentTime - lightingTime[i] >= lightingDuration[i] * 1000)
//       {
//         setRGB(String(i + 1) + ",0,0,0,0,0");
//         ledOn();
//       }
//     }

//   }
//   Serial.println("End shake Mode");
//   setAllRGB("0,0,0,0,0");
//   M5.dis.fillpix(LED_COLOR_RED);
// }

// void IRsensor()
// {
// #ifdef ARDUINO_ESP32C3_DEV
//   uint16_t kRecvPin = 32;
// #else
//   uint16_t kRecvPin = 32;
// #endif

//   uint32_t kBaudRate = 115200;
//   uint16_t kCaptureBufferSize = 1024;

// #if DECODE_AC
//   uint8_t kTimeout = 50;
// #else
//   uint8_t kTimeout = 15;
// #endif

//   uint16_t kMinUnknownSize = 12;
//   uint8_t kTolerancePercentage = kTolerance;

//   IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
//   decode_results results;

//   while (!Serial)
//     delay(50);
//   assert(irutils::lowLevelSanityCheck() == 0);

//   Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);
// #if DECODE_HASH
//   irrecv.setUnknownThreshold(kMinUnknownSize);
// #endif
//   irrecv.setTolerance(kTolerancePercentage);
//   irrecv.enableIRIn();

//   while (true)
//   {
//     // if (deviceConnected)
//     // {
//     //   std::string command = pCharacteristic->getValue();
//     if (Serial.available() > 0)
//     {
//       String command = getSerialCommand();
//       if (command == "end")
//       {
//         break;
//       }
//     }

//     if (irrecv.decode(&results))
//     {
//       uint32_t now = millis();
//       Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
//       if (results.overflow)
//         Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
//       Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");
//       if (kTolerancePercentage != kTolerance)
//         Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);
//       Serial.print(resultToHumanReadableBasic(&results));
//       String description = IRAcUtils::resultAcToString(&results);
//       if (description.length())
//         Serial.println(D_STR_MESGDESC ": " + description);
//       yield();

// #if LEGACY_TIMING_INFO
//       Serial.println(resultToTimingInfo(&results));
//       yield();
// #endif
//       Serial.println(resultToSourceCode(&results));
//       Serial.println();
//       yield();

//       if (results.value == 0xFF6897)
//       { // Replace with your specific IR code
//         // M5.dis.fillpix(LED_COLOR_YELLOW);
//         // delay(1000);
//         // M5.dis.fillpix(LED_COLOR_OFF);
//         waveAnimation();
//       }
//     }
//   }
//   Serial.println("End IR Mode");
//   setAllRGB("0,0,0,0,0");
// }
#endif //USE_OTHER_FUNCTIONS

// サンプルアニメーション
///////////////////////////////////////////////////////////////
void waveAnimation()
{
  M5.dis.fillpix(LED_COLOR_YELLOW);

  int waveLength = 11 / 2; // Define the length of the wave in terms of LED sets
  int wavePower = 50;           // Maximum brightness for the wave peak
  for (int cycle = 0; cycle < 3; cycle++)
  { // Number of wave cycles
    for (int step = 0; step < 11 + waveLength; step++)
    {
      for (int i = 0; i < 11; i++)
      {
        int distance = abs(step - i);
        if (distance < waveLength)
        {
          power[i] = map(distance, 0, waveLength, wavePower, 0);
        }
        else
        {
          power[i] = 0;
        }
        red[i] = power[i];   // Set a fixed color for simplicity, you can modify as needed
        green[i] = power[i]; // Set a fixed color for simplicity, you can modify as needed
        blue[i] = power[i];  // Set a fixed color for simplicity, you can modify as needed
      }
      ledOn();
      // printRGB();
      delay(200); // Adjust for desired wave speed
    }
  }
  M5.dis.fillpix(LED_COLOR_GREEN);
}

void randomAnimation()
{
  M5.dis.fillpix(LED_COLOR_YELLOW);

  for (int i = 0; i < 50; i++)
  { // Run the animation for 100 steps
    for (int j = 0; j < 11; j++)
    {
      power[j] = random(0, 50); // Random brightness
      red[j] = random(0, 50);   // Random red value
      green[j] = random(0, 50); // Random green value
      blue[j] = random(0, 50);  // Random blue value
    }
    ledOn();
    // printRGB();
    delay(100); // Adjust for desired twinkling speed
  }
  setAllRGB("0,0,0,0,0");
  // printRGB();
  M5.dis.fillpix(LED_COLOR_GREEN);
}

void shockAnimation()
{
  M5.dis.fillpix(LED_COLOR_YELLOW);

  for (int i = 0; i < 10; i++)
  {
    for (int i = 0; i < 11; i++)
    {
      power[i] = 50;            // Maximum brightness
      red[i] = random(0, 50);   // Random red value for each shock effect
      green[i] = random(0, 50); // Random green value for each shock effect
      blue[i] = random(0, 50);  // Random blue value for each shock effect
    }

    ledOn();
    // printRGB();
    delay(100); // Display the shock effect for a short time

    // Fade out effect
    for (int fade = 50; fade >= 0; fade -= 15)
    {
      for (int i = 0; i < 11; i++)
      {
        power[i] = fade;
      }
      ledOn();
      delay(100); // Adjust for desired fade speed
    }
  }
  setAllRGB("0,0,0,0,0");
  // printRGB();
  M5.dis.fillpix(LED_COLOR_GREEN);
}
