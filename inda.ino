#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

//tombol
#define BUZZER_PIN 17 // Pin buzzer terhubung ke pin GPIO 4
#define BUTTON_PIN 15 // Pin tombol terhubung ke pin GPIO 5
#define LED_PIN 19    // Pin LED terhubung ke pin GPIO 19
bool buzzerOn = false; // Variabel untuk melacak status bunyi buzzer

// Firebase & wifi
#define API_KEY "AIzaSyCKsCREBB79XbRUSx0xYGzBLz3x7oWUK6I"
#define DATABASE_URL "https://tinfit-7ed33-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData newData;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

#define MAX_DATA_COUNT 5 // Batas maksimum data


//data tombol
int buttonState = 1;

void setup(){

    Serial.begin(115200);
    WiFi.mode(WIFI_STA); 
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
    res = wm.autoConnect("Tinfit", "tinfit123"); // password protected ap

    if (!res)
    {
        Serial.println("Failed to connect");
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected");
       
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

    // tombol
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Mengaktifkan resistor pull-up internal pada pin tombol
    pinMode(LED_PIN, OUTPUT);
}

void loop(){
    kirimData();
    getData();
    kontrol();
}

void tombol() {
    buttonState = digitalRead(BUTTON_PIN);

    // Saat tombol ditekan
    if (buttonState == LOW)
    {
        delay(10); // Memberikan sedikit delay untuk menghindari bouncing

        // Memastikan tombol telah dilepas sebelum membaca status lagi
        while (digitalRead(BUTTON_PIN) == LOW)
        {
        }

        // Saat tombol ditekan, jika buzzer sedang mati, maka aktifkan bunyi buzzer dan nyalakan LED
        if (!buzzerOn)
        {
            digitalWrite(BUZZER_PIN, HIGH);
            digitalWrite(LED_PIN, HIGH);
            buzzerOn = true;
        }
        // Saat tombol ditekan lagi, jika buzzer sedang hidup, maka matikan bunyi buzzer dan LED
        else
        {
            digitalWrite(BUZZER_PIN, LOW);
            digitalWrite(LED_PIN, LOW);
            buzzerOn = false;
        }
    }
}

void kirimData() {
    if (Firebase.ready() && signupOK)
    {
        if (Firebase.RTDB.getInt(&fbdo, "tombol/data/count/count"))
        {
            int dataCount = fbdo.to<int>();

            if (dataCount >= MAX_DATA_COUNT) // Jika jumlah data sudah mencapai atau melebihi batas
            {
                // Hapus data pertama
                Firebase.RTDB.setString(&fbdo, "tombol/data/riwayat/0", ""); // Set the value at index 0 to an empty string to effectively remove it
            }

            // Geser data ke atas untuk membuat ruang untuk data baru
            for (int i = 0; i <= min(dataCount - 1, MAX_DATA_COUNT - 2); i++)
            {
                // Mendapatkan nilai pada indeks ke-i
                String path = "tombol/data/riwayat/" + String(i);
                Firebase.RTDB.getInt(&fbdo, path);
                int value = fbdo.intData();
                // Menyimpan nilai pada indeks ke-(i+1)
                path = "tombol/data/riwayat/" + String(i + 1);
                Firebase.RTDB.setInt(&fbdo, path, value);
            }

            // Tambahkan data baru ke indeks terakhir
            if (Firebase.RTDB.setInt(&fbdo, "tombol/data/riwayat/0", buttonState))
            {
                Serial.print("tombol: ");
                Serial.println(buttonState);
                // Update jumlah data
                Firebase.RTDB.setInt(&fbdo, "tombol/data/riwayat/count", min(dataCount + 1, MAX_DATA_COUNT)); // Gunakan nilai minimum antara dataCount + 1 dan MAX_DATA_COUNT untuk membatasi jumlah data
            }
            else
            {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
            }
        }
        else
        {
            Serial.println("Failed to get data count from Firebase.");
        }
    }
    Serial.println("__");
}

void getData() {
    if (Firebase.ready() && signupOK)
    {
        sendDataPrevMillis = millis();

        if (Firebase.RTDB.getInt(&fbdo, "tombol/data/kontrol/kontrol"))
        {
            aturTombol = fbdo.to<int>();
            println(aturTombol);
        }
        else
        {
            Serial.printf("Error getting data relay1: %s\n", fbdo.errorReason().c_str());
        }
     
    }
}

void kontrol(){
    if (aturTombol == 1) {
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
    } else if (aturTombol == 0) {
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
    }
}