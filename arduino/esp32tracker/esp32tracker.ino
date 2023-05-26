/*
    This sketch demonstrates how to scan WiFi networks.
    The API is almost the same as with the WiFi Shield library,
    the most obvious difference being the different file you need to include:
*/

/* Found orig code here: https://github.com/mdp/ESP32_DNS_Tracker

"The ESP32 device periodically scans it's WiFi environment and gets a list of the 10 strongest AP's and the MAC addressed. 
If one of those AP's is "open" (but can be a captive portal or paid wifi service), it connects to the AP, 
then encodes the list of 10 AP MAC addresses into a DNS query to a server we control. 
When we see that query, we decode it, get a list of the MAC addresses and use Google's Geocoding API to return 
an approximate location. Because of the ESP32's ability to deep-sleep, we can power a unit for several months off one battery.""
*/


#include "WiFi.h"
#include "Base32.h"
#include "DNS32.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define uS_TO_M_FACTOR 60000000  /* Conversion factor for micro seconds to minutes */
#define TIME_TO_SLEEP  1         /* Time ESP32 will go to sleep (in minutes) */
#define WIFI_CONNECT_TIMEOUT  10000 /* Milliseconds to timeout on wifi connect */
#define B32_DEVICE_ID  "ZACKAAAA"

RTC_DATA_ATTR int bootCount = 0;

int MAX_NETWORKS = 10;

// The FQDN for Hex DNS
//DNS32 dns32("i.mdp.im");
DNS32 dns32("ns1.dash9computing.com");

void printHex(byte ar[], int len)
{
  for (int i = 0; i < len; i ++) {
    Serial.printf("%02x", ar[i]);
    tft.printf("%02x", ar[i]);
  }
  Serial.print("\n");
  Serial.flush();
  tft.print("\n");
  tft.flush();
}

String mac2String(byte ar[]) {
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[3];
    sprintf(buf, "%02X", ar[i]);
    s += buf;
    if (i < 5) s += ':';
  }
  return s;
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);
  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor (ST77XX_RED);
  tft.print ("DASH"); 
  tft.setTextColor (ST77XX_YELLOW);
  tft.print ("9");    
  tft.setTextColor (ST77XX_BLUE);
  tft.print ("COMPUTING");
  tft.println();
  tft.println();
  tft.setTextColor(ST77XX_WHITE);
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  tft.println("Boot number: " + String(bootCount));


  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_M_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  tft.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  // Attempt to scan and report
  scanAndReport();
  //
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0,0);
  tft.setTextColor (ST77XX_BLUE);
  Serial.println("Going to sleep now");
  tft.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("Setup done");
  tft.println("Setup done");

}

bool writeId(int bootnumber, char* id)
{
  strcpy(id, B32_DEVICE_ID);
  for (int i = 8; i < 12; i++) {
    id[i] = DNS32::RFC4648_ALPHABET[random(32)];
  }
  id[12] = DNS32::RFC4648_ALPHABET[bootnumber % 32];
  return true;
}


int pickNetwork(int openNetworks, int totalNetworks)
{
  int networkIdx = 0; // Strongest AP

  // Bias towards the top strength network, but occasionally pick a weaker top 3
  int r = random(100) + 1;
  if (openNetworks >= 2) {
    if (r > 62) {
      networkIdx = 1;
    }
    if (openNetworks >= 3 && r > 86) {
      networkIdx = 2;
    }
  }

  // Go though the networks and find the nth open network
  int j = 0;
  for (int i = 0; i < totalNetworks && i < MAX_NETWORKS; ++i) {
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      if (j == networkIdx) {
        return i;
      }
    }
  }

}

// Return true if connected
bool connectToWifi(int networkIdx)
{
  int delayMs = 500;
  int timeoutCtr = 0;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);

  char ssid[33];
  WiFi.SSID(networkIdx).toCharArray(ssid, 33);

  WiFi.begin(ssid, NULL);
  while (WiFi.status() != WL_CONNECTED) {
    if (timeoutCtr * delayMs >= WIFI_CONNECT_TIMEOUT) {
      return false;
    }
    Serial.println("Connecting to WiFi..");
    tft.println("Connecting to WiFi..");

    timeoutCtr++;
    delay(delayMs);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Timed out connecting to %s\n", ssid);
    tft.printf("Timed out connecting to %s\n", ssid);

    return false;
  }
  Serial.printf("Connected to %s\n", ssid);
  tft.printf("Connected to %s\n", ssid);
  return true;
}

// Return true if connected
bool connectToHomeWifi(int networkIdx)
{
  int delayMs = 500;
  int timeoutCtr = 0;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  //WiFi.begin("PrivateNework", "password");

  while (WiFi.status() != WL_CONNECTED) {
    if (timeoutCtr * delayMs >= 8000) {
      return false;
    }
    Serial.println("Connecting to WiFi..");
    tft.println("Connecting to WiFi..");
    timeoutCtr++;
    delay(delayMs);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("Timed out connecting to Home Network");
    tft.printf("Timed out connecting to Home Network");
    return false;
  }
  return true;
}

// Call DNS with b32 after wifi connects
// Fail early if DNS queries fail.
void reportLocation(byte *b32, int len)
{
  Serial.printf("B32 Payload Len %d\n", len);
  tft.printf("B32 Payload Len %d\n", len);
  char str[len + 1];
  memcpy(str, b32, len);
  str[len] = '\0';
  int nQueries = dns32.getQueriesLen(str);
  Serial.printf("Number of Queries %d\n", nQueries);
  tft.printf("Number of Queries %d\n", nQueries);
  char id[13];
  writeId(bootCount, id);

  for (int q = 0; q < nQueries; q++)
  {
    char queryStr[254];
    int outLen = dns32.writeQuery(q, id, str, queryStr);
    Serial.println(queryStr);
    tft.println(queryStr);
    // Make DNS Query
    IPAddress result;
    int err = WiFi.hostByName(queryStr, result) ;
    if (err == 1) {
      Serial.print("Ip address: ");
      Serial.println(result);
      tft.print("Ip address: ");
      tft.println(result);
    } else {
      Serial.print("Error code: ");
      Serial.println(err);
      tft.print("Error code: ");
      tft.println(err);
      //delay(1000);
    }
  }
}

void scanAndReport()
{
  Serial.println("scan start");
  tft.println("scan start");
  Base32 base32;
  String jsonStr = String();
  int openNetworks = 0;
  byte reportData[1024];
  int rIdx = 0;

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  tft.println("scan done");
  delay(2500);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0,0);
  if (n == 0) {
    Serial.println("no networks found");
    tft.println("no networks found");

  } else {
    Serial.print(n);
    Serial.println(" networks found");
    tft.setTextSize(1);
    tft.setTextColor (ST77XX_RED);
    tft.print(n);
    tft.println(" NETWORKS FOUND");
    reportData[rIdx] = 0; rIdx++; // Version 0
    reportData[rIdx] = n > MAX_NETWORKS ? MAX_NETWORKS & 255 : n & 255; rIdx++;
    for (int i = 0; i < n && i < MAX_NETWORKS; ++i) {

      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.print(" C(");
      Serial.print(WiFi.channel(i));
      Serial.print(") ");
      Serial.print(mac2String(WiFi.BSSID(i)));
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      tft.setTextColor (ST77XX_GREEN);
      tft.print(i + 1);
      tft.print(": ");
      tft.print(WiFi.SSID(i));
      tft.print(" (");
      tft.print(WiFi.RSSI(i));
      tft.print(")");
      tft.print(" C(");
      tft.print(WiFi.channel(i));
      tft.print(") ");
      tft.print(mac2String(WiFi.BSSID(i)));
      tft.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
        openNetworks++;
      }
      for (int j = 0; j < 6; j++) {
        reportData[rIdx] = WiFi.BSSID(i)[j]; rIdx++;
      }
      reportData[rIdx] = WiFi.channel(i); rIdx++;
      reportData[rIdx] = WiFi.RSSI(i); rIdx++;
    }

  }
  delay(2500);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0,0);
  Serial.println("");
  tft.println("");
  printHex(reportData, rIdx);
  Serial.printf("Open Networks: %d\n", openNetworks);
  tft.printf("Open Networks: %d\n", openNetworks);
  if (openNetworks == 0) {
    return; // Nothing to do
  }

  int selectedOpenNetworkIdx = pickNetwork(openNetworks, n);
  String ssid = WiFi.SSID(selectedOpenNetworkIdx);
  for (int j = 0; j < ssid.length(); j++) {
    reportData[rIdx] = ssid.charAt(j); rIdx++;
  }

  bool connectedResult = connectToWifi(selectedOpenNetworkIdx);
  if (connectedResult) {
    byte* tmpB32 = NULL;

    int encodedLen = base32.toBase32(reportData, rIdx, (byte*&)tmpB32);
    Serial.printf("%.*s\n", encodedLen, tmpB32);
    tft.printf("%.*s\n", encodedLen, tmpB32);
    reportLocation(tmpB32, encodedLen);

    free(tmpB32);
  } else {
    Serial.println("Unable to connect to wifi network");
    tft.println("Unable to connect to wifi network");
  }
}

void loop() {}
