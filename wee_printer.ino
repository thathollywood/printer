#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

/* Includes for printer */
#include <SoftwareSerial.h>
#include <Thermal.h>

#define debug(a) Serial.print(millis()); Serial.print(": "); Serial.println(a);
#define debug2(a, b) Serial.print(millis()); Serial.print(": "); Serial.print(a); Serial.println(b);

const int greenPin = 6;
const int redPin = 7;

void initDiagnosticLEDs() {
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  digitalWrite(greenPin, HIGH);
  digitalWrite(redPin, HIGH);
  delay(100);
  digitalWrite(greenPin, LOW);
  digitalWrite(redPin, LOW);
}

const int printer_RX_Pin = 2;  // this is the green wire
const int printer_TX_Pin = 3;  // this is the yellow wire
Thermal printer(printer_RX_Pin, printer_TX_Pin);
const int postPrintFeed = 3;

void initPrinter() {
  printer.begin(150);
}

const int SD_Pin = 4;

void initSD() {
  // stop the SD stuff futzing.
  pinMode(SD_Pin, OUTPUT);
  SD.begin(SD_Pin);
}

byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x86, 0x67 }; //physical mac address
EthernetClient client;

void initNetwork() {
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    debug("DHCP Failed");
    // no point in carrying on, so do nothing forevermore:
    while(true);
  }
  // print your local IP address:
  debug2("IP address: ", Ethernet.localIP());
}

void setup(){
  Serial.begin(9600);

  initPrinter();
  initSD();
  initNetwork();
  initDiagnosticLEDs();
}

const char* host = "192.168.1.22";
const int port = 4567;
const char *path = "/1";

int failures = 0;
int requests = 0;

void loop() {  
  digitalWrite(greenPin, LOW);

  if (client.connect(host, port)) {
    requests++;
    uint32_t content_length = 0;

    debug("Connected.");
    client.print("GET "); client.print(path); client.println(" HTTP/1.0\n"); 
    uint32_t length = 0;
    if (SD.exists("TMP")) { SD.remove("TMP"); }
    File cache = SD.open("TMP", FILE_WRITE);

    while(client.connected()) {
      debug("Still connected");
      // the server will close the connection when it is finished sending packets

      client.find("Content-Length: ");
      char c;
      while (isdigit(c = client.read())) {
        content_length = content_length*10 + (c - '0');
      }
      client.find("\r\n\r\n");
      
      debug2("Content length was: ", content_length);

      debug("Now reading data");
      while(client.available()) {
        // ...but not until this buffer is empty
        cache.write(client.read());
        length++;
      }
      debug("Done reading for now...");
    }
    // close your end after the server closes its end
    client.stop();
    cache.close();

    cache = SD.open("TMP");

    if (cache.size() != content_length) {
      debug2("File size: ", cache.size());
      failures++;
    } else {
      digitalWrite(greenPin, HIGH);
      if (content_length > 0) {
        printer.printBitmap(&cache);
        printer.feed(postPrintFeed);
      }
    }

    cache.close();

    if (failures > 0) {
      digitalWrite(redPin, HIGH);
    }
    debug2("Total read: ", length);
    Serial.print("Total failures: "); Serial.print(failures); Serial.print("/"); Serial.println(requests);
  } else {
    debug("Couldn't connect.");
  }

  delay(1000);
}