#ifndef IFTTTTrigger_h
#define IFTTTTrigger_h

#include <WiFi.h>
#include <HTTPClient.h>

const char* iftttKey = "b7ZuwquYwTsP7k4-8BHWzW";  // Your IFTTT key
const char* eventName = "milk_machine_error";  // Event Name

void triggerIFTTT() {
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;

    String url = String("https://maker.ifttt.com/trigger/") + eventName + "/with/key/" + iftttKey;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json"); //Specify content-type header

    int httpResponseCode = http.POST("{\"value1\":\"\",\"value2\":\"\"}"); //Send the actual POST request

    if (httpResponseCode > 0) {
      String response = http.getString(); //Get the response to the request
      Serial.println(httpResponseCode);   //Print return code
      Serial.println(response);           //Print request answer
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end(); //Free resources
  } else {
    Serial.println("Error in WiFi connection");
  }
}

#endif //IFTTTTrigger_h
