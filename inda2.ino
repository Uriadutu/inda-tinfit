#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Firebase & wifi

#define API_KEY "AIzaSyB9EPM5GQZPlqamI0zrbUIXPGplP655J0A"
#define DATABASE_URL "https://nodemcu-fb601-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData newData;
const size_t dataLimit = 10;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

// Update data
#define MAX_DATA_COUNT 5 // Batas maksimum data
int lampu1 = 19;

// DHT
#define DHTPIN 18
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float t, h;

// Ketinggian Air
int FloatSensor = 32;
int buttonState = 1;

// SUHU AIR
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
int temperatureC;

// Kontrol
int relay1, relay2, relay3, otomatis, cluster;
int pompaAir = 5;
int pompaNutrisi = 19;
int semprotKabut = 26;

// TDS
#define TdsSensorPin 34 /// TDS
#define VREF 3.3        // analog reference voltage(Volt) of the ADC
#define SCOUNT 30

int analogBuffer[SCOUNT]; // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float averageVoltage = 0;
int tdsValue = 0;
float temperature = 27; // current temperature for compensation

// Median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen)
{
    int bTab[iFilterLen];

    for (byte i = 0; i < iFilterLen; i++)
        bTab[i] = bArray[i];

    int i, j, bTemp;

    for (j = 0; j < iFilterLen - 1; j++)
    {
        for (i = 0; i < iFilterLen - j - 1; i++)
        {
            if (bTab[i] > bTab[i + 1])
            {
                bTemp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = bTemp;
            }
        }
    }

    if ((iFilterLen & 1) > 0)
    {
        bTemp = bTab[(iFilterLen - 1) / 2];
    }
    else
    {
        bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
    }

    return bTemp;
}

//==============================================//

// setup
void setup()
{
    Serial.begin(115200);
    dht.begin();
    sensors.begin();
    pinMode(TdsSensorPin, INPUT);
    pinMode(FloatSensor, INPUT_PULLUP);
  pinMode(lampu1, OUTPUT);

    // LCD ===============
    lcd.begin();
    lcd.backlight();

    // Koneksi Wifi & Firebase
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);

    // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("Hidroponik", "hidroponikppl"); // password protected ap

    if (!res)
    {
        Serial.println("Failed to connect");
        // ESP.restart();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Failed to connect");
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wifi connected");
    }

    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("ok");
        signupOK = true;
    }
    else
    {
        Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    pinMode(pompaAir, OUTPUT);
    pinMode(pompaNutrisi, OUTPUT);
    pinMode(semprotKabut, OUTPUT);
    digitalWrite(pompaAir, HIGH);
    digitalWrite(pompaNutrisi, HIGH);
    digitalWrite(semprotKabut, HIGH);
}

// Loop
void loop()
{


    // Kontrol
    firebaseReceiveData();
    kontrlomanualRelay();
}

// firebase terima data
void firebaseReceiveData()
{
    if (Firebase.ready() && signupOK)
    {
        sendDataPrevMillis = millis();

        if (Firebase.RTDB.getInt(&fbdo, "hidro/kontrol/relay/relay1/relay1"))
        {
            relay1 = fbdo.to<int>();
        }
        else
        {
            Serial.printf("Error getting data relay1: %s\n", fbdo.errorReason().c_str());
        }

        if (Firebase.RTDB.getInt(&fbdo, "hidro/kontrol/relay/relay2/relay2"))
        {
            relay2 = fbdo.to<int>();
        }
        else
        {
            Serial.printf("Error getting data relay2: %s\n", fbdo.errorReason().c_str());
        }

        if (Firebase.RTDB.getInt(&fbdo, "hidro/kontrol/relay/relay3/relay3"))
        {
            relay3 = fbdo.to<int>();
        }
        else
        {
            Serial.printf("Error getting data relay3: %s\n", fbdo.errorReason().c_str());
        }

        if (Firebase.RTDB.getInt(&fbdo, "hidro/kontrol/auto/auto"))
        {
            otomatis = fbdo.to<int>();
        }
        else
        {
            Serial.printf("Error getting data otomatis: %s\n", fbdo.errorReason().c_str());
        }

        if (Firebase.RTDB.getInt(&fbdo, "hidro/cluster/lastDataCluster/lastDataCluster"))
        {
            cluster = fbdo.to<int>();
            Serial.print("data cluster : ");
            Serial.print(cluster);
        }
        else
        {
            Serial.printf("Error getting data cluster: %s\n", fbdo.errorReason().c_str());
        }
    }
}

// Kontrol manual

void kontrlomanualRelay()
{
    if (relay1 == 1)
    {
        digitalWrite(lampu1, HIGH);
    }
    else
    {
        digitalWrite(lampu1, LOW);
    }
    if (relay2 == 1)
    {
        digitalWrite(pompaNutrisi, LOW);
    }
    else
    {
        digitalWrite(pompaNutrisi, HIGH);
    }

    if (relay3 == 1)
    {
        digitalWrite(semprotKabut, LOW);
    }
    else
    {
        digitalWrite(semprotKabut, HIGH);
    }

    Serial.println();
    Serial.println("-------------------");
    Serial.println();
}
