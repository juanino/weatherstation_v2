#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#include <EEPROM.h>;
#define EEPROM_SIZE 2 // 2 bytes for an int?

unsigned long uptime = 0;  // Variable to store the uptime in milliseconds
unsigned long rebootTime = 8L * 60L * 60L * 1000L;  // 8 hours in milliseconds

int buttonpin = 13;
int program_number = 0;

// program 0 | default
// program 1 | temp only
// program 2 | nite light only
// program 3 | no display
// prorgam 4 | unused
// program 5 | unused

// setup struct for all the weather data
// to pass to other functions
struct WeatherData {
  String temp;
  String feel;
  String humidity;
  String wind;
  bool error;
};

// get configuration from another file
#include "config.h"

Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 secondalpha4 = Adafruit_AlphaNum4();

void setup() {
  uptime = millis();  // Record the current uptime (time since boot)
  Serial.begin(9600);
  Serial.println("sleeping");
  delay(1000);
  EEPROM.begin(EEPROM_SIZE);
  int program_saved = 0;
  EEPROM.get(0,program_saved);
  program_number = program_saved; // set program number to one from memory on restart
  Serial.print("reading 2 bytes to get program number saved in memory:");
  Serial.println(program_saved);

  pinMode(buttonpin, INPUT_PULLUP);  // setup programming button

  alpha4.begin(0x70);        // pass in the address
  secondalpha4.begin(0x74);  // pass in the address

  alpha4.clear();
  alpha4.writeDisplay();

  // Connect to WiFi access point.
  Serial.println();
  Serial.println();
  // Print the MAC address
  String macAddress = WiFi.macAddress();  // Get the MAC address as a String
  Serial.print("MAC Address: ");
  Serial.println(macAddress);

  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);
  scroll_msg(WLAN_SSID);

  WiFi.begin(WLAN_SSID,WLAN_PASS); // or add , WLAN_PASS if you use a password
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // Get the local IP address (IPAddress object)
  IPAddress localIP = WiFi.localIP();

  // Convert IP address to char[] (C-style string)
  char myip[16];  // An IP address in dotted decimal form has a maximum of 15 characters + null terminator
  sprintf(myip, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  // Pass the IP address as a char* to the function
  scroll_msg2(myip);  // Pass 'myip' which is of type 'char*'
  Serial.print("ip address:");
  Serial.println(myip);
  delay(500);



  scroll_msg(intro_msg);
  scroll_msg2(intro_msg2);
  delay(1000);
  scroll_msg("ROC ");
  scroll_msg2(" WX ");
  delay(1000);
  scroll_msg("LOADING LAST PROGRAM");
  char programStr[10];                        // Array to hold the converted string
  dtostrf(program_number, 4, 0, programStr);  // Convert the rounded integer to a string (no decimal places)
  scroll_msg2(programStr);
}

// MAIN LOOP
// --------------------
void loop() {
  WeatherData w = fetch_weather();
  if (w.error = 1) {
    // retry fetch_weather once more after small delay
    delay(2000);
    w.error = 0;
    fetch_weather();
  }

  unsigned long startMillis = millis();  // Capture the starting time
  unsigned long duration = 600000;       // Duration for 10 minutes (in milliseconds)

  while (millis() - startMillis < duration) {
    print_weather(w);  // Print the weather
    Serial.println("reading button....");
    read_button();
    // Add a short delay between prints (for readability or display update rate)
    //delay(1000);  // Delay for 1 second (you can adjust this as needed)
  }
  Serial.println("Timer loop done! Exiting loop...fetching data again");


}
// -----------------------------------
// END MAIN LOOP
// -----------------------------------

void scroll_msg(String msg) {
  Serial.println("scrolling message: ");
  Serial.println(msg);
  Serial.println("length of msg is:");
  size_t msglen = msg.length();
  Serial.println(msglen);
  if (msglen < 4) {
    msg = "SHRT";
  }
  for (int i = 0; i < (msglen - 3); i++) {
    alpha4.writeDigitAscii(0, msg[i]);
    alpha4.writeDigitAscii(1, msg[i + 1]);
    alpha4.writeDigitAscii(2, msg[i + 2]);
    alpha4.writeDigitAscii(3, msg[i + 3]);
    alpha4.writeDisplay();
    if (msglen > 4) {
      delay(300);
    }
    delay(2);
  }
}

void scroll_msg2(String msg) {
  Serial.println("scrolling message: ");
  Serial.println(msg);
  Serial.println("length of msg is:");
  size_t msglen = msg.length();
  Serial.println(msglen);
    if (msglen < 4) {
    msg = "SHRT";
  }
  for (int i = 0; i < (msglen - 3); i++) {
    secondalpha4.writeDigitAscii(0, msg[i]);
    secondalpha4.writeDigitAscii(1, msg[i + 1]);
    secondalpha4.writeDigitAscii(2, msg[i + 2]);
    secondalpha4.writeDigitAscii(3, msg[i + 3]);
    secondalpha4.writeDisplay();
    delay(300);
  }
}



WeatherData fetch_weather() {
  read_button();
  WeatherData w;
  WiFiClient client;
  if (!client.connect(server, 80)) {
    Serial.println("connection failed");
    w.error = 1;
    return w;
  }

  // We now create a URI for the request
  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + server + "\r\n" + "Connection: close\r\n\r\n");
  delay(500);
  // Read all the lines of the reply from server and print them to Serial
  //while(client.available()){
  //  String line = client.readStringUntil('\r');
  //  Serial.print(line);
  //}
  // Capture the response (discard headers and capture JSON body)
  String response = "";
  bool isJsonStarted = false;  // Flag to indicate when the JSON starts

  while (client.available()) {
    String line = client.readStringUntil('\r');
    if (line.indexOf("{") >= 0) {
      isJsonStarted = true;  // Start capturing JSON after finding '{'
    }

    if (isJsonStarted) {
      response += line;  // Append the JSON data to the response string
    }
  }

  // Close the connection to the server
  client.stop();

  // Now we have the full JSON response in the `response` string
  Serial.println("Received Response: ");
  Serial.println(response);


  // Now parse the JSON
  DynamicJsonDocument doc(1024);  // Create a DynamicJsonDocument of suitable size

  // Deserialize the JSON response
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.f_str());
    w.error = 1;
    w.temp = "ERR-";
    w.feel = "ERR-";
    w.humidity = "ERR-";
    w.wind = "ERR-";
    return w;
  }

  // Extract specific fields from the JSON object
  float temperature = doc["main"]["temp"];
  float feelsLike = doc["main"]["feels_like"];
  int humidity = doc["main"]["humidity"];
  const char* weatherDescription = doc["weather"][0]["description"];
  float windSpeed = doc["wind"]["speed"];
  int windDeg = doc["wind"]["deg"];

  // round temps
  int temperatureInt = round(temperature);  // Round the temperature to the nearest integer
  int feelInt = round(feelsLike);
  Serial.print("feelInt is:");
  Serial.println(feelInt);
  Serial.print("tempint is:");
  Serial.println(temperatureInt);

  // temp
  char temperatureStr[10];                        // Array to hold the converted string
  dtostrf(temperatureInt, 4, 0, temperatureStr);  // Convert the rounded integer to a string (no decimal places)
  w.temp = temperatureStr;

  // feels like
  char feelStr[10];
  dtostrf(feelInt, 4, 0, feelStr);
  w.feel = feelStr;

  // humidity
  char humidityStr[10];
  dtostrf(humidity, 4, 0, humidityStr);
  w.humidity = humidityStr;

  // wind
  char windStr[10];
  dtostrf(windSpeed, 4, 0, windStr);
  w.wind = windStr;

  return w;
  // make func
}

void print_weather(WeatherData w) {
  check_uptime();
  // check the program number to run
  if (program_number == 3) {
    // no display
    scroll_msg("    ");
    scroll_msg2("    ");
    return;
  }

  if (program_number == 2) {
    scroll_msg(night_light);
    scroll_msg2(night_light);
    delay(5);
    return;
  }

  Serial.print("temp is: ");
  Serial.println(w.temp);
  scroll_msg("TEMP");
  scroll_msg2(w.temp);
  if (program_number == 1) {
    // only print temp and temp and exit
    return;
  }

  delay(1000);

  Serial.print("feel is: ");
  Serial.println(w.feel);
  scroll_msg("FEEL");
  scroll_msg2(w.feel);

  delay(1000);

  Serial.print("humidty is: ");
  Serial.println(w.humidity);
  scroll_msg("-RH-");
  scroll_msg2(w.humidity);

  delay(1000);

  Serial.print("wind speed is:");
  Serial.println(w.wind);
  scroll_msg("WIND");
  scroll_msg2(w.wind);

  delay(1000);
}

void read_button() {
  Serial.println("checking button..........");
  Serial.println(digitalRead(buttonpin));
  if (digitalRead(buttonpin) == 0) {
    scroll_msg("PGRM");
    scroll_msg2("----");
    int pg_mode_loops = 0;
    // you are now in program mode
    while (true) {
      pg_mode_loops++;
      if (digitalRead(buttonpin) == 0) {
        // check to see how long we are in program mode
        Serial.print("program loop number:");
        Serial.println(pg_mode_loops);
        Serial.print("program number is:");
        Serial.println(program_number);
        program_number++;
        // start over when you hit program 5
        if (program_number > 5) {
          program_number = 0;
        }
        char program_string[5];  // Array size to hold the result (4 characters + null terminator)
        // Format with width 4, 0 decimals (no decimal point), and leading spaces if necessary
        dtostrf(program_number, 4, 0, program_string);  // The 4 ensures it's at least 4 characters wide
        scroll_msg2(program_string);
        delay(400);
      }
      delay(5);
      if (pg_mode_loops > 800) {
        scroll_msg("SAVE");
        scroll_msg2("SAVE");
        EEPROM.put(0,program_number);
        EEPROM.commit();
        delay(300);
        return;
      }
    }  // end while
  }    // end if
}  // end read_button

void check_uptime() {
  // Check if the ESP8266 has been up for 8 hours
  Serial.print("-----------------------------------uptime is:");
  Serial.println(millis() - uptime);
  if (millis() - uptime >= rebootTime) {

    Serial.println("8 hours elapsed. Rebooting ESP8266...");
    ESP.restart();  // Reboot the ESP8266
  }
}