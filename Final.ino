#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <HX711.h>
#include <DHT.h>
#include <SoftwareSerial.h>

// Constants
const int LOADCELL_DOUT_PIN = A0;
const int LOADCELL_SCK_PIN = A1;
const int DHT_PIN = 4;
const int GSM_RX = 2;
const int GSM_TX = 3;
const int RELAY_PIN = 5;

const float WEIGHT_THRESHOLD = 100.0; // Set the weight threshold
const float TEMP_LOW = 25.0; // Set the low temperature threshold
const float TEMP_HIGH = 40.0; // Set the high temperature threshold

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {12, 11, 10, 9};
byte colPins[COLS] = {8, 7, 6};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD, HX711, DHT11 and GSM module initialization
LiquidCrystal_I2C lcd(0x27, 16, 2);
HX711 scale;
DHT dht(DHT_PIN, DHT11);
SoftwareSerial A6GSM(GSM_RX, GSM_TX);

String incomingMessage;
bool readMessageBody = false;
bool messageBodyNextLine = false;

const String masterPass = "8485";

bool oneTime = true;

int otp;

void setup() {
  Serial.begin(9600);
  Serial.println("Setup Started...");
  A6GSM.begin(9600);

  Serial.println("GSM Set to Text mode");
  A6GSM.println("AT+CMGF=1");
  delay(1000);
  Serial.println("Delete all sms");
  A6GSM.println("AT+CMGD=1, 4");
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  dht.begin();
  lcd.init();
  lcd.backlight();
  scale.set_scale(); // Set the calibration factor for your specific load cell
  scale.tare(); // Reset the scale to 0
 // lcd.setCursor(0, 0);
  //lcd.print("Delivery Hub Box");
  Serial.println("Setup Finished!");
}

void loop() {
  checkTemperature();
  checkSMS();
  weightcheck();


  String enteredPassword = getPasswordFromKeypad();
  Serial.println(enteredPassword);

  if (enteredPassword == "8485") {
    Serial.println("Password passed");
    oneTime = false;
    lcd.setCursor(0, 1);
    //lcd.print("Locker is Open");
    digitalWrite(RELAY_PIN, HIGH);
    delay(10000);
    digitalWrite(RELAY_PIN, LOW);
  }

  if (enteredPassword == String (otp)) {
    if (oneTime)
    {
      Serial.println("OTP passed");
      oneTime = false;
      lcd.setCursor(0, 0);
      //lcd.print("Locker is Open");
      digitalWrite(RELAY_PIN, HIGH);
      delay(10000);
      digitalWrite(RELAY_PIN, LOW);
    }
  }
  

 

}

String getPasswordFromKeypad() {
  String enteredPassword = "";
  char key = keypad.getKey();

  if (key) {
    lcd.clear();
    lcd.print("Enter password:");

    while (enteredPassword.length() < 4) {
      key = keypad.getKey();
      if (key) {
        enteredPassword += key;
        //        enteredPassword = enteredPassword.toInt();
        //        Serial.println(enteredPassword);
        lcd.setCursor(enteredPassword.length(), 1);
        lcd.print("*");
        delay(200);
      }
    }
    lcd.clear();
  }

  return enteredPassword;
}

void checkSMS() {
  while (A6GSM.available()) {
    String response = A6GSM.readStringUntil('\n');
    Serial.println(response); // Debugging: print the raw response

    if (response.indexOf("+CMTI:") >= 0) { // Check if the response contains an incoming SMS notification
      int indexStart = response.indexOf(',') + 1;
      int index = response.substring(indexStart).toInt();

      A6GSM.print("AT+CMGR=");
      A6GSM.println(index); // Read the SMS message at the extracted index
      readMessageBody = true;
    } else if (readMessageBody && response.indexOf("+CMGR:") >= 0) {
      messageBodyNextLine = true;
    } else if (readMessageBody && messageBodyNextLine) {
      incomingMessage = response;
      processMessage(incomingMessage);
      readMessageBody = false;
      messageBodyNextLine = false;
    }
  }
}
void weightcheck(){
   float weight ;
     weight = scale.get_units();
    float w=weight/(500);
// Serial.print("Pruduct weight: ");
// Serial.print(weight/(-600));
//  Serial.println(" g");
  lcd.setCursor(0, 0);
   lcd.print(" ")
;  lcd.print("Weight: ");
  lcd.print(abs(w));
  
  lcd.print('g');
  delay(500);
  if (abs(w)>200) {
   
    sendWeightSMS();
  
  }
   }

void checkTemperature() {
  float temperature = dht.readTemperature();

  //  Serial.print("Temp: ");
  //  Serial.println(temperature);
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temperature);
  delay(100);

  if (temperature < TEMP_LOW || temperature > TEMP_HIGH) {
    String tempAlert = "Temperature alert: " + String(temperature) + "C";
    sendSMS(tempAlert);
    lcd.setCursor(0, 1);
    lcd.print(tempAlert);
    delay(2000);
    lcd.clear();
  }
}

void processMessage(String message) {
  String userRole, phoneNumber;

  if (message.indexOf("OTP") >= 0) {
    userRole = "Delivery";
    // } else if (message.indexOf("Generate OTP for Customer") >= 0) {
    //userRole = "Customer";
  } else {
    // Invalid message format, ignore
    Serial.println("Invalid message format");
    return;
  }

  int numberIndex = message.indexOf("Number:") + 8;
  phoneNumber = message.substring(numberIndex, numberIndex + 11);

  otp = generateOTP();
  delay(1000);
  Serial.println("OTP is: ");
  Serial.println(otp);
  sendOTP(userRole, phoneNumber, otp);
}

int generateOTP() {
  return random(1001, 9998);
  delay(1000);
}

void sendOTP(String userRole, String phoneNumber, int otp) {
  Serial.println("Sending SMS");
  A6GSM.print("AT+CMGS=\"");
  A6GSM.print(phoneNumber);
  A6GSM.println("\"");
  delay(1000);

  A6GSM.print("Your ");
  A6GSM.print(userRole);
  A6GSM.print(" OTP is: ");
  A6GSM.print(otp);
  A6GSM.print("\n");
  A6GSM.print("Use this OTP to unlock the hub.\n");
  A6GSM.print("This OTP will only work once.");
  A6GSM.write((byte)0x1A); // Send Ctrl+Z to send the SMS

  Serial.print("SMS Sent to this phone number: ");
  Serial.println(phoneNumber);
  Serial.print("The user role is: ");
  Serial.println(userRole);
  Serial.print("And the OTP is: ");
  Serial.println(otp);
  Serial.println("Use this OTP to unlock the box door. This OTP will only work once.");
  Serial.println("SMS Sent.");
  delay(1000);
  A6GSM.print("AT+CMGD=1,4");
  delay(100);
  Serial.println("Delete all messages");
}

void sendSMS(String message) {
  Serial.println("Sendig SMS...");
  A6GSM.print("AT+CMGF=1\r");
  delay(100);
  A6GSM.print("AT+CMGS=\"01724460948\"\r"); // Replace with the phone number to receive the SMS
  delay(100);
  A6GSM.print(message);
  delay(100);
  A6GSM.write((byte)0x1A);
  delay(100);
  Serial.println("SMS Sent.");
}

void sendWeightSMS() {
  Serial.println("Sendig SMS...");
  A6GSM.print("AT+CMGF=1\r");
  delay(100);
  A6GSM.print("AT+CMGS=\"01724460948\"\r"); // Replace with the phone number to receive the SMS
  delay(100);
  A6GSM.print("Overweight Alert!\n");
  delay(100);
  A6GSM.print("The product weight crossed it's limit! The door can't be locked.");
  delay(100);
  A6GSM.write((byte)0x1A);
  delay(100);
  Serial.println("SMS Sent.");
}
