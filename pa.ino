#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Servo.h> // Sertakan library Servo
#include <PubSubClient.h> // Sertakan library MQTT

// Kredensial WiFi
const char* ssid = "wifi";
const char* password = "1sampai8";

// Token Bot Telegram (dari BotFather)
const char* BotToken = "7273517179:AAElCMuI_HKfrPRKrog45ArGQ1J8hhZrAYM";
const String CHAT_ID = "1844302374";

// Pengaturan MQTT
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// Pengaturan Pin
const int trigPin = D7;
const int echoPin = D8;
const int servoPin = D4; // Pin untuk servo

unsigned long duration; 
int distance; //menyimpan hasil perhitungan jarak yang diperoleh dari duration. Jarak ini dinyatakan dalam satuan sentimeter (cm).
bool motionDetected = false; // untuk menunjukkan apakah gerakan telah terdeteksi atau tidak. 
bool doorOpen = false; // Status pintu, awalnya tertutup

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BotToken, secured_client);
unsigned long lastUpdateTime = 0; // Waktu terakhir pembaruan dicek

Servo myServo; // Buat objek Servo untuk mengontrol servo

unsigned long lastMotionTime = 0; // Waktu terakhir gerakan terdeteksi
const unsigned long motionInterval = 30000; // Interval deteksi gerakan dalam milidetik

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("connecting WiFi...");
  }

  Serial.println("Terhubung ke WiFi");
  secured_client.setInsecure(); // Gunakan baris ini jika Anda tidak ingin berurusan dengan sertifikat

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  myServo.attach(servoPin); // Pasang servo ke pin servoPin
  myServo.write(0); // Inisialisasi servo ke posisi tertutup

  // Koneksi ke MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// Fungsi callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected");
      // Once connected, publish an announcement...
      client.publish("/indobot/p/mqtt", "Indobot"); // publish data ke topik mqtt
      // ... and resubscribe
      client.subscribe("/indobot/p/mqtt"); // subscribe data ke mqtt broker
    } else {
      Serial.print("failed, rc="); //Menghubungkan kembali ke server MQTT jika koneksi terputus.
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds"); //Mencoba menghubungkan setiap 5 detik jika gagal.
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  detectMotion();

  // Publish data jarak ke broker MQTT
  String distanceStr = String(distance); //mengirimkan pesan ke mqtt berupa string
  client.publish("fitra/dht", distanceStr.c_str(), 2); //menghubungkan ke mqtt menggunakan topic fitra/dht

  // Cek pesan baru dari Telegram
  if (millis() - lastUpdateTime > 100) {  
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    for (int i = 0; i < numNewMessages; i++) {
      String chat_id = String(bot.messages[i].chat_id);
      String text = bot.messages[i].text;

      if (text.equals("/start")) {
        String response = "Selamat datang! Silakan pilih perintah:\n/buka - Buka pintu\n/tutup - Tutup pintu\n/status - Tampilkan status pintu";
        bot.sendMessage(chat_id, response, "");
        Serial.println("Perintah start diterima");
      } else if (text.equals("/buka")) {
        Serial.println("Perintah membuka diterima");
        myServo.write(180); // Buka pintu (servo bergerak ke 180 derajat)
        doorOpen = true; // Set status pintu
        String response = "Pintu dibuka.";
        bot.sendMessage(chat_id, response, "");
        Serial.println(response); // Tampilkan pesan di Serial Monitor
      } else if (text.equals("/tutup")) {
        Serial.println("Perintah menutup diterima");
        myServo.write(0); // Tutup pintu (servo bergerak ke 0 derajat)
        doorOpen = false; // Set status pintu
        String response = "Pintu ditutup.";
        bot.sendMessage(chat_id, response, "");
        Serial.println(response); // Tampilkan pesan di Serial Monitor
      } else if (text.equals("/status")) {
        Serial.println("Perintah status diterima");
        String response = doorOpen ? "Pintu terbuka." : "Pintu tertutup.";
        bot.sendMessage(chat_id, response, "");
        Serial.println(response); // Tampilkan pesan di Serial Monitor
      }
    }
    lastUpdateTime = millis();
  }
}

void detectMotion() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Tetapkan batas waktu untuk pulseIn ke 20 ms (20000 mikrodetik)
  duration = pulseIn(echoPin, HIGH, 20000); // Timeout ditetapkan ke 20 ms
  distance = duration * 0.034 / 2; // Hitung jarak dalam sentimeter

  unsigned long currentTime = millis(); // Menyimpan waktu saat ini dalam milidetik
  if (distance > 0 && distance < 200) { //Memeriksa apakah jarak yang terukur berada dalam rentang 0 hingga 200 cm
    if (currentTime - lastMotionTime > motionInterval) {
      Serial.println("Gerakan Terdeteksi!!!"); // Tambahkan pernyataan ini

      if (!motionDetected) { // Hanya kirim pesan jika belum mendeteksi gerakan sebelumnya
        // Beri tahu deteksi gerakan melalui Telegram
        String message = "Gerakan Terdeteksi!!!\nSilahkan pilih aksi:\n/buka - Buka pintu\n/tutup - Tutup pintu";
        bot.sendMessage(CHAT_ID, message, "");
        motionDetected = true; // Set motionDetected ke true
      }
      lastMotionTime = currentTime; // Perbarui waktu terakhir gerakan terdeteksi
    }
  } else {
    motionDetected = false; // Reset motionDetected jika tidak ada gerakan
  }
}
