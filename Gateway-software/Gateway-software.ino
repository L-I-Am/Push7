#include "WiFi.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <EEPROM.h>

#define MAX_CHAR_SIZE 100

const char* ssid = "Dash7-gateway";

char client_ssid_string[100];
int ssid_length;

char client_password_string[100];
int password_length;

#define MAGIC_NUMBER 255

#define WIFI_TIMEOUT 20000
#define WIFI_DELAY_RETRY 500
WebServer server(80);
bool posted = false;
bool first_connect = true;

void handleRoot();
void handlePost();

void write_credentials_to_eeprom()
{
  int offset = 0;
  EEPROM.write(offset, MAGIC_NUMBER);
  offset += 1;
  EEPROM.write(offset, ssid_length);
  offset += 1;
  for(int i = 0; i < ssid_length; i++) {
    EEPROM.write(offset, client_ssid_string[i]);
    offset += 1;
  }
  EEPROM.write(offset, password_length);
  offset += 1;
  for(int i = 0; i < password_length; i++) {
    EEPROM.write(offset, client_password_string[i]);
    offset += 1;
  }
}

void init_credentials_eeprom()
{
  int offset = 0;
  if(EEPROM.read(offset) != MAGIC_NUMBER) {
    ssid_length = 0;
    password_length = 0;
  }
  offset += 1;

  ssid_length = EEPROM.read(offset);
  offset += 1;
  for(int i = 0; i < ssid_length; i++) {
    client_ssid_string[i] = EEPROM.read(offset);
    offset += 1;
  }

  password_length = EEPROM.read(offset);
  offset += 1;
  for(int i = 0; i < password_length; i++) {
    client_password_string[i] = EEPROM.read(offset);
    offset += 1;
  }
}

void setup() 
{
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect();

  WiFi.softAP(ssid);

  server.begin();
  MDNS.begin("window");

  init_credentials_eeprom();
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/change", HTTP_POST, handlePost);
  server.onNotFound(handleRoot);
}

bool check_connection() {
  if(WiFi.status() != WL_CONNECTED) {
    int n = WiFi.scanNetworks();
    if(n < 1)
      return false;
    bool found = false;
    for(int i = 0; i < n; i++) {
      String found_ssid = WiFi.SSID(i);
      char* found_ssid_char;
      int len = found_ssid.length() + 1;
      found_ssid.toCharArray(found_ssid_char, len);
      if(!memcmp(found_ssid_char, client_ssid_string, len)) {
        char pw[password_length];
        memcpy(pw, client_password_string, password_length);
        WiFi.begin(found_ssid_char, pw);
      }
    }
    if(!found)
      return false;

    int total_delay = 0;
    while((WiFi.status() != WL_CONNECTED) && (total_delay < WIFI_TIMEOUT)) {
      delay(WIFI_DELAY_RETRY);
      total_delay += WIFI_DELAY_RETRY;
    }
    
    return WiFi.status() == WL_CONNECTED;
  } else
    return true;
}

void loop() 
{
  if(check_connection()) {
    if(first_connect) {
      first_connect = false;
    }
    server.handleClient();
  }
}

const String first = "<form action=\"change\" method=\"POST\"><div><table width=\"100%\">";
const String postedString = "<tr><th colspan=\"3\" style=\"color: red\">Command sent to server!</th></tr>";
const String WiFiCredentialsString = "<tr><th colspan=\"3\">Wi-Fi SSID</th></tr><tr><th colspan=\"3\"><input type=\"text\" name=\"SSID\"></th></tr>"
                                     "<tr><th colspan=\"3\">Password</th></tr><tr><th colspan=\"3\"><input type=\"password\" name=\"password\"></th></tr>"
                                     "<tr></tr><tr><th colspan=\"3\"><input type=\"submit\" value=\"submit\"></th></tr></table></div></form>";

void handleRoot() {
  String htmlPage = first; 
  if(posted)
    htmlPage += postedString;
  htmlPage += WiFiCredentialsString;

  server.send(200, "text/html", htmlPage);

  posted = false;
}

void handlePost() {
  if(server.hasArg("SSID")) {
    String ssid = server.arg("SSID");
    ssid_length = ssid.length();
    char* found_ssid_char;
    ssid.toCharArray(found_ssid_char,ssid_length); 
    memcpy(client_ssid_string, found_ssid_char, ssid_length);
  }
  if(server.hasArg("password")) {
    String pw = server.arg("password");
    password_length = pw.length();
    char* found_pw_char;
    pw.toCharArray(found_pw_char,password_length); 
    memcpy(client_password_string, found_pw_char, password_length);
  }

  if(ssid_length > 0)
    write_credentials_to_eeprom();

  posted = true;
  handleRoot();
}
