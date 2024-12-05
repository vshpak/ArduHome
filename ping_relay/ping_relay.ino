/*
Ping Controller - to monitor Internet-connection status and reboot 4G-modem if required.

Ref.docs:
https://github.com/andrew-susanto/Arduino-Ethernet-Icmp
https://forum.amperka.ru/threads/%D0%9F%D1%80%D0%BE%D1%85%D0%BE%D0%B6%D0%B4%D0%B5%D0%BD%D0%B8%D0%B5-%D0%BF%D0%B8%D0%BD%D0%B3%D0%B0-%D0%BF%D1%80%D0%B8-%D0%BF%D0%BE%D0%BC%D0%BE%D1%89%D0%B8-uno-ethernet-shield.15630/#post-282225
https://habr.com/ru/companies/timeweb/articles/709986/
https://3d-diy.ru/wiki/arduino-platy/ethernet-shield/
https://www.youtube.com/watch?v=CvqHkXeXN3M
https://drive.google.com/drive/folders/1E5ZFuQEGaYo0WWZtB1dDfoYZNuC6uvE-
*/

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetICMP.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[]={0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte empty_ip[]={0,0,0,0};
byte ip[]={192,168,1,70};
byte dns[]={8,8,8,8};
byte gtway[]={192,168,1,1};
byte subnet[]={192,168,1,0};

// ICMP stuff
IPAddress pingAddr(77,88,55,242); // ip address to ping
IPAddress emptyAddr(0,0,0,0);

SOCKET pingSocket = 0;
char buffer [256];
EthernetICMPPing ping(pingSocket, (uint16_t)random(0, 255));

int PING_INTERVAL = 10000; // 10 seconds
int MAX_FAILS = 3; // max count of failing before 4G-modem reboot
int FAIL_COUNT = 0; // count of failing
int MAX_REBOOTS = 3; // max count of failing before 4G-modem reboot
int REBOOTS_COUNT = 0; // count of failing

// constants for LEDs
int LED_RED = 7;
int LED_YELLOW = 6;
int LED_GREEN = 8;
int LED_BLUE = 5;

// constants for relay
int AC_RELAY_CH1 = 2;
int AC_RELAY_CH2 = 3;

LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display

int ping_reply;
int ping_time;


void setup() {
  // LEDs init
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  // LCD init
  lcd.init();         // initialize the lcd
  lcd.backlight();    // Turn on the LCD screen backlight
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  print_init(); //print init info
  network_check(); // check network
  modem_turn_on(); // turn on 4G-modem
}


void loop() {
  EthernetICMPEchoReply echoReply = ping(pingAddr, 4);
  if (echoReply.status == SUCCESS)
    print_ping_success(echoReply);
  else
    print_ping_fail(echoReply);

  // too many failed echo requests - rebooting 4G-modem
  Serial.println(buffer);
  if (FAIL_COUNT >= MAX_FAILS) {
    modem_reboot();
  }
  delay(PING_INTERVAL);
}


void print_init(){
  Serial.println("Ping Controller is alive...");
  lcd.setCursor(0, 0);
  lcd.print("Ping Controller");
  lcd.setCursor(4, 1);
  lcd.print(" is alive...");
  delay(5000);
}

void network_check() {
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, dns, gtway, subnet);
  delay(2000);
  if(Ethernet.localIP() != emptyAddr) {
    Serial.print("Local IP set correctly:  ");
    Serial.println(Ethernet.localIP());
    digitalWrite(LED_GREEN, HIGH);  //Green status LED turn on
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP is correct:");
    lcd.setCursor(0, 1);
    lcd.print(Ethernet.localIP());
    delay(3000);
  }
  else {
    Serial.print("ERROR!!! Local IP is empty:  ");
    Serial.println(Ethernet.localIP());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP set ERROR!!!");
    lcd.setCursor(0, 1);
    lcd.print(Ethernet.localIP());
    while (true) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_YELLOW, HIGH);
      delay(500);
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YELLOW, LOW);
      delay(500);
    }
  }
}

void print_ping_success(EthernetICMPEchoReply echoReply) {
  sprintf(buffer,
    "Reply[%d] from: %d.%d.%d.%d: bytes=%d time=%ldms TTL=%d",
    echoReply.data.seq,
    echoReply.addr[0],
    echoReply.addr[1],
    echoReply.addr[2],
    echoReply.addr[3],
    REQ_DATASIZE,
    millis() - echoReply.data.time,
    echoReply.ttl);
  ping_time = millis() - echoReply.data.time;
  Serial.println(ping_time);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reply:");
  lcd.setCursor(7, 0);
  //lcd.print(echoReply.data.seq);
  ping_reply = echoReply.data.seq;
  lcd.print(echoReply.data.seq);
  lcd.setCursor(0, 1);
  lcd.print("Time:");
  lcd.setCursor(6, 1);
  lcd.print(ping_time);
  FAIL_COUNT = 0;
  digitalWrite(LED_BLUE, LOW);
  delay(150);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YELLOW, LOW);
}

void print_ping_fail(EthernetICMPEchoReply echoReply) {
  sprintf(buffer, "Echo request failed!"); 
  if (FAIL_COUNT < 3 ) {
    FAIL_COUNT++;
    digitalWrite(LED_RED, LOW);
    delay(300);
    digitalWrite(LED_RED, HIGH);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Echo request");
  lcd.setCursor(5, 1);
  lcd.print("failed!");
  lcd.setCursor(15, 1);
  lcd.print(FAIL_COUNT);
  delay(300);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_RED, HIGH);
}

void modem_turn_on() {
  pinMode(AC_RELAY_CH1, OUTPUT);
  pinMode(AC_RELAY_CH2, OUTPUT);
  Serial.println("RELAY ON");
  digitalWrite(AC_RELAY_CH1, LOW);
  digitalWrite(AC_RELAY_CH2, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Turning on");
  lcd.setCursor(4, 1);
  lcd.print("the modem...");
  for (int i = 0; i <= 100; i++) {
    digitalWrite(LED_BLUE, HIGH);
    delay(400);
    digitalWrite(LED_BLUE, LOW);
    delay(400);
    }
}

void modem_reboot() {
  // turn off the modem
  digitalWrite(LED_YELLOW, HIGH);
  delay(2000);
  Serial.println("RELAY OFF");
  lcd.clear();
  lcd.setCursor(7, 0);
  lcd.print("Rebooting");
  lcd.setCursor(4, 1);
  lcd.print("the modem...");
  digitalWrite(AC_RELAY_CH1, HIGH);
  digitalWrite(AC_RELAY_CH2, HIGH);
  // wait for a while
  for (int i = 0; i <= 10; i++) {
    digitalWrite(LED_YELLOW, HIGH);
    delay(500);
    digitalWrite(LED_YELLOW, LOW);
    delay(500);
    }
  // turn on the modem
  digitalWrite(AC_RELAY_CH1, LOW);
  digitalWrite(AC_RELAY_CH2, LOW);
  Serial.println("RELAY ON");
  digitalWrite(LED_YELLOW, HIGH);
  delay(1000);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  // reset count of failed attempts
  FAIL_COUNT = 0;
  REBOOTS_COUNT++;
  Serial.print("The modem rebooted ");
  Serial.print(REBOOTS_COUNT);
  Serial.println(" times");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Modem rebooted");
  lcd.setCursor(0, 1);
  lcd.print(REBOOTS_COUNT);
  lcd.setCursor(11, 1);
  lcd.print("times");
  // wait for turning on the modem
  Serial.println("Waiting for 90 secs to let the modem start work");
  for (int i = 0; i <= 100; i++) {
    digitalWrite(LED_BLUE, HIGH);
    delay(400);
    digitalWrite(LED_BLUE, LOW);
    delay(400);
  }
}

