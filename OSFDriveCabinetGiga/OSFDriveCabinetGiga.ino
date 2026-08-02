/*
 =====================================================================
 * OSF GV6K Configuration GUI for Arduino Giga with Display Shielf
 * Original Author: Colt McGuire
 * Source/Repository: https://github.com/OSFauto/Gemini_Configurator
 * Date Updated: 07/31/2026
 * Notes: 
 =====================================================================
 */

// Include all required libraries and external files
#include "Configs.h"                                    // Custom defined header file of the motor configs (made using python script)
#include "Arduino_H7_Video.h"                           // Arduino library for display
#include "Arduino_GigaDisplayTouch.h"                   // Arduino library for touch interface
#include "lvgl.h"                                       // Embedded graphics library
#include "ui.h"                                         // UI created using Squareline Studio
#include <stdio.h>                                      // Standard C++ library

// SETUP GIGA
Arduino_H7_Video Display(800, 480, GigaDisplayShield);  // Define the giga display 
Arduino_GigaDisplayTouch Touch;                         // Define the touch interface
GDTpoint_t touchPoints[5];                              // Define the array of touch points

// SLEEP
bool isSleeping = false;
const int SLEEP_DELAY_MS = 30000;                       // Sleep after 30 seconds
unsigned long lastActivityTime = 0;                     // Track the last time there was an interaction

// CONNECTION PINGING
const int CHECK_CONNECTION_MS = 20000;                  // Check connection every 10 seconds
const int TIMEOUT_TIME_MS = 1000;                       // If we dont get a response in 1 seconds, we have timed out
unsigned long lastConnectedTime = 0;                    // Track the last time we were connected (check connection)
unsigned long connectionFirstCheck = 0;                 // Track the time we started checking connection (timeout time)
unsigned long connectionLastCheck = 0;                  // Track the time between connection checks

// COMMUNICATION INTERLOCK (mostly unimplimented as it was causing more problems than solving)
bool communicating = false;                             // Are we currently sending or expecting a command
String pendingCommand = "";                             // String to store overlapping commands

// VAR USED IN MULTIPLE SCREENS
String driveName = "";                                  // Name of the drive (U12, etc)
bool connected = false;                                 // Are we connected or not
String motorType = "";                                  // Name of the current configured motor (DMTR)
String ipAddress = "";                                  // IP Address from the drive
const int logSize = 10;                                 // Number of logs to save
int curLog = 0;                                         // Current index in log array to achieve a looping list
String logs[logSize];                                   // Array of logs

// VAR USED IN IPCONFIG
char newIpAddress[][4] = {"---","---","---","---"};     // Array of the user inputted ip address fields
int currentField = 3;                                   // The ip address field currently selected (default to the final field)

// VAR USED IN MOTOR CONFIG
String selectedMotorConfig = "MPP115 3C1E";             // String of the selected motor config option from the dropdown
bool sentConfig = false;                                // Bool to prevent double sending motor config

// VAR USED IN DRIVE STATUS
String driveStatusPrint = "";                           // Drive status info to display on the status screen


// Setup Variables, UI, and Connection
void setup() {
  // Begin Serial Communication
  Serial.begin(9600);
  delay(1000);

  // Begin display and touch interface
  Display.begin();
  Touch.begin();

  // Initialize UI and load startup screen
  ui_init();
  lv_scr_load(ui_StartupScreen);
  
  // Setup UI Elements
  lv_label_set_text(ui_LoadingScreenText, "Loading UI Elements");
  lv_timer_handler();   // Call to refresh display before loop is running
  setupEventHandlers();
  setupUIOptions();
  
  // Allow recoloration of sections of the ip label
  lv_label_set_recolor(ui_IPDisplay, true);

  // Setup Serial communication
  lv_label_set_text(ui_LoadingScreenText, "Starting Serial Communication");
  lv_timer_handler();
  // Begin Serial2(blue-19tx, yellow-18rx) to communicate with drive
  Serial2.begin(9600);

  // Attempt to connect to drive
  lv_label_set_text(ui_LoadingScreenText, "Connecting to Drive");
  lv_timer_handler();
  establishContact();

  // Read initial drive info and set UI values
  lv_label_set_text(ui_LoadingScreenText, "Retrieving Drive Info");
  lv_timer_handler();
  getDriveInfo();
  lv_timer_handler();

  // Load Home Screen once setup is complete
  lv_scr_load(ui_HomeScreen);
  lv_timer_handler();
  // Start sleep timer once setup is complete
  unsigned long lastActivityTime = millis();
}

// Attach event handler callback functions to all interactable widgets
void setupEventHandlers()
{
  // HOME SCREEN BUTTONS
  lv_obj_add_event_cb(ui_IPConfigButtonHome, IPConfig_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_MotorConfigButtonHome, MotorConfig_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_DriveStatusButtonHome, DriveStatus_evt_handler, LV_EVENT_CLICKED, NULL);

  // IP CONFIG SCREEN BUTTONS
  lv_obj_add_event_cb(ui_HomeButtonIPConfig, Home_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_SendButtonIPConfig, SendIP_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_IPConfigPrevFieldButton, IPPrevField_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_IPConfigNextFieldButton, IPNextField_evt_handler, LV_EVENT_CLICKED, NULL);
  // IP CONFIG SCREEN ROLLERS
  lv_obj_add_event_cb(ui_IPDigit1, SetIPDigit1_evt_handler, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_IPDigit2, SetIPDigit2_evt_handler, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_IPDigit3, SetIPDigit3_evt_handler, LV_EVENT_VALUE_CHANGED, NULL);

  // MOTOR CONFIG SCREEN BUTTONS
  lv_obj_add_event_cb(ui_HomeButtonMotorConfig, Home_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_SendButtonMotorConfig, SendMotorConfig_evt_handler, LV_EVENT_CLICKED, NULL);
  // MOTOR CONFIG SCREEN DROPDOWN
  lv_obj_add_event_cb(ui_MotorConfigSelect, SetMotorConfig_evt_handler, LV_EVENT_VALUE_CHANGED, NULL);

  // DRIVE STATUS SCREEN BUTTONS
  lv_obj_add_event_cb(ui_HomeButtonStatus, Home_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_EnaNetworkButtonStatus, EnableNetwork_evt_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_FactoryResetButtonStatus, FactoryReset_evt_handler, LV_EVENT_CLICKED, NULL);
}

// Ensure selectable fields and hidden objects are set how we want them
void setupUIOptions()
{
  // IP Config
  lv_roller_set_options(ui_IPDigit1, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n-", LV_ROLLER_MODE_INFINITE);
  lv_roller_set_options(ui_IPDigit2, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n-", LV_ROLLER_MODE_INFINITE);
  lv_roller_set_options(ui_IPDigit3, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n-", LV_ROLLER_MODE_INFINITE);
  lv_obj_add_flag(ui_SpinnerIPConfig, LV_OBJ_FLAG_HIDDEN);

  // MOTOR Config
  lv_dropdown_set_options(ui_MotorConfigSelect, "MPP115 3C1E\nMPP100 3D1E\nMPP142 4D1E\nBE232 DJ\nBE233 DJ\nBE343 JJ");
}

// Initiate handshake with GV6K
void establishContact() {
  // Send "ECHO" to drive until we receive ECHO1 confirmation (meaning the drive is communicating)
  String response = "";
  while(response != "ECHO,*ECHO1")
  {
    lv_timer_handler();
    // Verify connection with GV6K
    Serial.println("connecting...");
    // If Serial2 has not sent data back, send check again and delay .3s
    while (Serial2.available() <= 0) {
      lv_timer_handler();
      Serial2.println("echo");
      delay(300);
    }
    // Read the response from the drive
    response = receiveData();
  }
  // Once connected, update connected time and update labels
  Serial.println("connected");
  connected = true;
  lastConnectedTime = millis();
  // Update UI
  lv_label_set_text(ui_StatusLabelHome, "Connected");
  lv_label_set_text(ui_StatusLabelIPConfig, "Connected");
  lv_label_set_text(ui_StatusLabelMotorConfig, "Connected");
  lv_label_set_text(ui_StatusLabelStatus, "Connected");
  
  
}

// Check if connection is still running
void checkConnection()
{
  // if this is the first time running the function this checking cycle
  if(connectionFirstCheck == 0)
  {
    connectionFirstCheck = millis();
    // if we have been disconnected, update UI
    if (connected == false)
    {
      lv_label_set_text(ui_StatusLabelHome, "Not Connected");
      lv_label_set_text(ui_StatusLabelIPConfig, "Not Connected");
      lv_label_set_text(ui_StatusLabelMotorConfig, "Not Connected");
      lv_label_set_text(ui_StatusLabelStatus, "Not Connected");
    }
    // if we arent disconnected yet, update UI 
    else
    {
      lv_label_set_text(ui_StatusLabelHome, "Connecting...");
      lv_label_set_text(ui_StatusLabelIPConfig, "Connecting...");
      lv_label_set_text(ui_StatusLabelMotorConfig, "Connecting...");
      lv_label_set_text(ui_StatusLabelStatus, "Connecting...");
    }
    // Send connection check
    Serial2.println("echo");
    connectionLastCheck = millis();
  }
  // Read Response if available
  if(Serial2.available() > 0)
  {
    // Check for expected response
    String response = receiveData();
    if(response == "ECHO,*ECHO1")
    {
      // SUCCESS - Update ui and tracking variables
      connectionFirstCheck = 0;
      communicating = false;
      connected = true;
      lv_label_set_text(ui_StatusLabelHome, "Connected");
      lv_label_set_text(ui_StatusLabelIPConfig, "Connected");
      lv_label_set_text(ui_StatusLabelMotorConfig, "Connected");
      lv_label_set_text(ui_StatusLabelStatus, "Connected");
      lastConnectedTime = millis();
      return;
    }
  }
  // Wait 300ms between connection checks per connection check cycle
  if(millis() - connectionLastCheck > 300)
  {
    Serial2.println("echo");
    connectionLastCheck = millis();
  }
  // Once time after first check of the cycle exceeds timeout time, connection check has failed
  if(millis() - connectionFirstCheck > TIMEOUT_TIME_MS)
  {
    // FAIL - Update UI and tracking variables
    connected = false;
    communicating = false;
    connectionFirstCheck = 0;
    lv_label_set_text(ui_StatusLabelHome, "Not Connected");
    lv_label_set_text(ui_StatusLabelIPConfig, "Not Connected");
    lv_label_set_text(ui_StatusLabelMotorConfig, "Not Connected");
    lv_label_set_text(ui_StatusLabelStatus, "Not Connected");
    logInfo("Connection Timeout");
    return;
  }
  
}

// Read drive type, motor type, ip config, and other required details
void getDriveInfo()
{
  // Send all check commands at once and recieve response as one string
  sendCommand("TREV:TNT:DMTR:CMDDIR:SGINTE:LJRAT:LDAMP");
  String setupData = receiveSetupData();
  // Split response into individual command response for easy parsing
  String TREV = setupData.substring(setupData.indexOf("TREV"), setupData.indexOf(","));
  String TNT = setupData.substring(setupData.indexOf("TNT"), setupData.indexOf(", DMTR"));
  // Get value responses for simple commands
  String DMTR = setupData.substring(setupData.indexOf("*DMTR")+5, setupData.indexOf(", CMDDIR"));
  String CMDDIR = setupData.substring(setupData.indexOf("*CMDDIR")+7,setupData.indexOf(", SGINTE"));
  String SGINTE = setupData.substring(setupData.indexOf("*SGINTE")+7,setupData.indexOf(", LJRAT"));
  String LJRAT = setupData.substring(setupData.indexOf("*LJRAT")+6,setupData.indexOf(", LDAMP"));
  String LDAMP = setupData.substring(setupData.indexOf("*LDAMP")+6,setupData.indexOf(", ,"));
  String temp = "";

  // Process TREV
  TREV.trim();
  // Save landmark indices
  int GVIdx = TREV.indexOf("  ")+8;
  int TREVIdx = TREV.indexOf("*TREV");
  // OS
  driveStatusPrint += "OS: "+TREV.substring(TREVIdx+5,TREV.indexOf(" ", TREVIdx+5))+"\n";
  // Drive type
  temp = TREV.substring(GVIdx,TREV.indexOf(" ", GVIdx));
  temp.remove(temp.length()-1);
  driveName = temp;
  driveStatusPrint += "Drive Type: ";
  driveStatusPrint += driveName;
  driveStatusPrint += "\n";
  driveName = driveName.substring(driveName.indexOf('-')+1);
  // Firmware
  driveStatusPrint += "Firmware Version: "+TREV.substring(TREV.indexOf(" ", GVIdx)+1,TREV.lastIndexOf(" "))+"\n";
  // Flash Boot
  driveStatusPrint += "Flash Boot Revision: "+TREV.substring(TREV.lastIndexOf(" ")+1)+"\n";

  // Process TNT
  // Parse TNT response into each ip field and map into newIpAddress array
  temp = TNT.substring(TNT.indexOf("*GEM6K IP address:")+19, TNT.indexOf(",",TNT.indexOf("*GEM6K IP address:")));
  temp.substring(0, temp.indexOf(".")).toCharArray(newIpAddress[0], sizeof(newIpAddress[0]));
  temp.substring(temp.indexOf(".")+1, temp.indexOf(".",4)).toCharArray(newIpAddress[1], sizeof(newIpAddress[1]));
  temp.substring(temp.indexOf(".",4)+1, temp.lastIndexOf(".")).toCharArray(newIpAddress[2], sizeof(newIpAddress[2]));
  temp.substring(temp.lastIndexOf(".")+1).toCharArray(newIpAddress[3], sizeof(newIpAddress[3]));
  // Save last field of ip as a number for future processing
  int lastAreaIp = temp.substring(temp.lastIndexOf(".")+1).toInt();
  ipAddress = temp;
  TNT.replace("*", "");
  TNT.replace(',', '\n');
  driveStatusPrint += TNT;
  driveStatusPrint += "\n";

  // Process DMTR
  String motorTypeTemp = motorType;
  // Map DMTR response to motor type names
  if(DMTR == "1803")
  {
    motorTypeTemp = "BE232 DJ";
  }
  else if(DMTR == "1804")
  {
    motorTypeTemp = "BE233 DJ";
  }
  else if(DMTR == "1852")
  {
    motorTypeTemp = "BE343 JJ";
  }
  else if(DMTR == "2722")
  {
    motorTypeTemp = "MPP100 3D1E";
  }
  else if(DMTR == "2736")
  {
    motorTypeTemp = "MPP115 3C1E";
  }
  else if(DMTR == "2752")
  {
    motorTypeTemp = "MPP142 4B1E";
  }
  else
  {
    logInfo("ERR: Unknown Motor Type");
    motorTypeTemp = "-------";
  }
  motorType = motorTypeTemp;

  // Append CMDDIR, SGINTE, LJRAT, and LDAMP to drive status
  driveStatusPrint += "CMDDIR: "+CMDDIR+"\nSGINTE: "+SGINTE+"\nLJRAT: "+LJRAT+"\nLDAMP: "+LDAMP;

  // -----UPDATE UI-------
  // Home
  char ipBuffer[16];
  ipAddress.toCharArray(ipBuffer, 16);
  lv_label_set_text(ui_IPConfigButtonHomeLabel, ipBuffer);
  char motorBuffer[14];
  motorType.toCharArray(motorBuffer, 14);
  lv_label_set_text(ui_MotorConfigButtonHomeLabel, motorBuffer);
  char driveNameBuffer[5];
  driveName.toCharArray(driveNameBuffer, 5);
  lv_label_set_text(ui_DriveNameMotorConfig, driveNameBuffer);
  lv_label_set_text(ui_DriveNameIPConfig, driveNameBuffer);
  lv_label_set_text(ui_DriveNameStatus, driveNameBuffer);
  lv_label_set_text(ui_DriveName, driveNameBuffer);

  // IPConfig
  currentField = 3;
  setIpLabel();
  // Get each digit of the last field for ip input
  // replace leading zeros with "-" (index 10)
  int d1 = lastAreaIp / 100 == 0? 10: lastAreaIp / 100; // get 100s place
  int d2 = (lastAreaIp % 100) / 10; // get 10s place
  int d3 = lastAreaIp % 10; // get 1s place
  // set rollers
  lv_roller_set_selected(ui_IPDigit1, d1, LV_ANIM_OFF);
  lv_roller_set_selected(ui_IPDigit2, d2, LV_ANIM_OFF);
  lv_roller_set_selected(ui_IPDigit3, d3, LV_ANIM_OFF);

  // Drive Status
  char statusBuf[500];
  driveStatusPrint.toCharArray(statusBuf, sizeof(statusBuf));
  lv_textarea_set_text(ui_StatusReadout, statusBuf);
}

// Called every frame
void loop() {
  // Handle UI updates
  lv_timer_handler();
  
  // Wake up on touch
  if (Touch.getTouchPoints(touchPoints) > 0) {
    lastActivityTime = millis();
    if (isSleeping) {
      isSleeping = false;
      lv_scr_load(ui_HomeScreen);
    }
  }
  
  // Sleep if inactive for set time
  if (millis() - lastActivityTime > SLEEP_DELAY_MS && !isSleeping) {
    isSleeping = true;
    lv_scr_load(ui_SleepScreen);
  }

  // Check connection after set time
  if (millis() - lastConnectedTime > CHECK_CONNECTION_MS)
  {
    checkConnection();
  }
}

// Write given string to display logs and update UIs
void logInfo(String log)
{
  // Print log to Serial monitor if pc is connected
  Serial.println(log);

  // Create a wrapping log array
  logs[curLog] = log;
  curLog++;
  if(curLog >= logSize)
  {
    curLog = 0;
  }

  // Loop through all logs in array and add them to the display with most recent on the bottom
  String logText = "";
  for(int i = 0; i <logSize; i++)
  {
    int idx = curLog+i < logSize? curLog+i : curLog+i-logSize;
    logText += logs[idx] == ""?"":"\n";
    logText += logs[idx];
    
  }
  // Refresh log displays
  char buf[logText.length()+10];
  logText.toCharArray(buf, sizeof(buf));
  lv_textarea_set_text(ui_ConsoleReadIPConfig, buf);
  lv_textarea_set_text(ui_ConsoleReadMotorConfig, buf);
  lv_textarea_set_text(ui_ConsoleReadStatus, buf);
}

// Sends command object over serial to drive
void sendCommand(Command cmd)
{
  // Lockout commands if a command is already in process
  if(communicating && pendingCommand == "")
  {
    pendingCommand = cmd.ToString();
    logInfo("communication in progress, adding to pending cue: " + pendingCommand);
    return;
  }
  // Clear previous buffer
  Serial2.readString();
  // Send command to drive and log the send command
  communicating = true;
  Serial.println(cmd.ToString());
  Serial2.println(cmd.ToString());
  logInfo("SENT: " + cmd.ToString());
  // Clear communication lockout
  communicating = false;
  pendingCommand = "";
}

// Sends command string over serial to drive
void sendCommand(String cmd)
{
  // Lockout commands if a command is already in process
  if(communicating && pendingCommand == "")
  {
    pendingCommand = cmd;
    logInfo("communication in progress, adding to pending cue: " + pendingCommand);
  }
  // Clear previous buffer
  Serial2.readString();
  // Send command to drive as well as to serial monitor
  communicating = true;
  Serial2.println(cmd);
  logInfo("SENT: " + cmd);
  // Clear communication lockout
  communicating = false;
  pendingCommand = "";
}

// Receive the multi-command response from drive status check
String receiveSetupData()
{
  // Communication lockout
  if(communicating)
  {
    logInfo("already communicating; returning from receive");
    return "ERR; Already Communicating";
  }
  communicating = true;

  String result = "";
  int count = 0;
  bool startOfLine = false;
  bool firstN = false;
  bool end = false;
  unsigned long lastCheck = millis();
  // Allow multiple iterations to recieve response
  while (!end && count <= 300)
  {
    // Check every 25ms
    if(millis() - lastCheck < 25)
    {
      continue;
    }
    lastCheck = millis();
    // Process pending serial data
    while (Serial2.available() > 0)
    {
      // Update UI and process current character
      lv_timer_handler();
      char curChar = Serial2.read();
      // If end has been reached, clean up result and return it
      if(end)
      {
        Serial2.readString();
        result.replace(",,", ",");
        result.replace(">","");
        if(result.indexOf("ECHO") == -1)
        {
          logInfo("RECEIVED: "+result);
        }
        communicating = false;
        return result;
      }
      // Perform action based on type of character received
      switch(curChar) 
      {
        // Carriage return character received
        case '\r':
          break;
        // Newline character received
        case '\n':
          // If we have received two \n characters, it is the end of the transmission
          if(firstN)
          {
            end = true;
          }
          // If this is the first \n in a row, end the line but keep receiving data
          else
          {
            // Add comma between lines
            result += ',';
            startOfLine = false;
            firstN = true;
          }
          break;
        // Non special character received, add to the data as is and clear any received newline character count
        default:
          firstN = false;
          result += curChar;
          break;
      }
      
      
    }
    count++;
  }
  logInfo("ERR; FAILED TO REACH END OF MESSAGE; RECEIVED: "+result);
  communicating = false;
  return result;
}

// Receive command responses
String receiveData()
{
  // Communication lockout
  if(communicating)
  {
    logInfo("already communicating; returning from receive");
    return "ERR; Already Communicating";
  }
  communicating = true;

  String result = "";
  int count = 0;
  bool startOfLine = false;
  bool firstR = false;
  bool end = false;
  unsigned long lastCheck = millis();
  // Allow multiple iterations to recieve response
  while (!end && count <= 300)
  {
    // Check every 25ms
    if(millis() - lastCheck < 25)
    {
      continue;
    }
    lastCheck = millis();
    // Process pending serial data
    while (Serial2.available() > 0)
    {
      // Process current character
      char curChar = Serial2.read();
      // If end has been reached, clean up result and return it
      if(end)
      {
        Serial2.readString();
        result.replace(",,", ",");
        result.replace(">","");
        if(result.indexOf("ECHO") == -1)
        {
          logInfo("RECEIVED: "+result);
        }
        communicating = false;
        return result;
      }
      // Perform action based on type of character received
      switch(curChar) 
      {
        // Carriage return character received
        case '\r':
          // If this is the first \r in a row, flip bool
          if (!firstR)
          {
            // first return
            firstR = true;
          }
          // If we have received two \r characters, it is the end of the transmission
          else
          {
            // second return in a row, we are at EOT
            end = true;
            firstR = false;
          }
          break;
        // Newline character received
        case '\n':
          if(!firstR)
          {
            end=true;
          }
          else
          {
            result += ',';
          }
          startOfLine = false;
          firstR = false;
          break;
        // Non special character received, add to the data as is and clear any received newline character count
        default:
          firstR = false;
          result += curChar;
          break;
      }
    }
    count++;
  }
  logInfo("ERR; FAILED TO REACH END OF MESSAGE; RECEIVED: "+result);
  communicating = false;
  return result;
}

// Send Motor Config
bool sendConfig(Command config[], int configSize)
{
  if(communicating)
  {
    logInfo("Mid communication, please try again");
  }
  logInfo("Sending Config...");
  String configStr = "";
  bool resultBool = true;
  String result = "";
  // Go through config list
  for (int i=0; i<configSize; i++)
  {
    configStr += config[i].ToString();
    configStr += ":";
    if(i >= configSize-1 || configStr.length() + config[i+1].ToString().length()+1 >= 95)
    {
      logInfo("Sending and checking config batch...");
      sendCommand(configStr);
      result = receiveData();
      result.replace(",","");
      result.replace(" ","");
      resultBool = resultBool && result.substring(0,result.lastIndexOf(":")).equalsIgnoreCase(configStr.substring(0,configStr.lastIndexOf(":")));
      configStr = "";
    }
  }
  if(resultBool)
  {
    // If no checks failed, config was a success
    logInfo("Motor Configured Successfully");
  }
  else
  {
    logInfo("ERROR: Motor configuration failed");
  }
  return resultBool;
}

bool configMotor(String motorType)
{
  bool success;

  // Send matching config type to selected motor type
  if(motorType == "MPP115 3C1E")
  {
    success = sendConfig(MPP115_3C1E_Config, MPP115_3C1E_Config_length);
  }
  else if(motorType == "MPP100 3D1E")
  {
    success = sendConfig(MPP100_3D1E_Config, MPP100_3D1E_Config_length);
  }
  else if(motorType == "MPP142 4D1E")
  {
    success = sendConfig(MPP115_3C1E_Config, MPP115_3C1E_Config_length);
  }
  else if(motorType == "BE232 DJ")
  {
    success = sendConfig(BE232DJ_Config, BE232DJ_Config_length);
  }
  else if(motorType == "BE233 DJ")
  {
    success = sendConfig(BE232DJ_Config, BE232DJ_Config_length);
  }
  else if(motorType == "BE343 JJ")
  {
    success = sendConfig(BE343JJ_Config, BE343JJ_Config_length);
  }
  else
  {
    success = false;
  }
  // Refresh drive info and send DRESET
  getDriveInfo();
  sendCommand("DRESET");
  receiveData();
  return success;
}

bool configNetwork(String ip)
{
    if(communicating)
  {
    logInfo("Mid communication, please try again");
    return false;
  }
  // Set IP
  sendCommand("NTADDR"+ip);
  String addrResp = receiveData();
  // Enable Ethernet
  sendCommand("NTFEN1");
  String ntfenResp = receiveData();
  sendCommand("RESET");
  receiveData();
  establishContact();
  return true;
}

void setIpLabel()
{
  String ipDisplay = "";
  for (int i = 0; i < 4; i++)
  {
    if(currentField == i)
    {
      ipDisplay += (String)"#00ff00 "+(String)newIpAddress[i]+(i == 3? "#" : "#.");
    }
    else
    {
      ipDisplay += (String)newIpAddress[i]+(i==3?"":".");
    }
  }
  char ipBuf[30];
  ipDisplay.toCharArray(ipBuf, sizeof(ipBuf));
  lv_label_set_text(ui_IPDisplay, ipBuf);
}

// Go to HomeScreen
void Home_evt_handler(lv_event_t * e)
{
  lv_scr_load(ui_HomeScreen);
}

// Go to IPConfigScreen
void IPConfig_evt_handler(lv_event_t * e)
{
  lv_scr_load(ui_IPConfigScreen);
}

// Go to MotorConfigScreen
void MotorConfig_evt_handler(lv_event_t * e)
{
  lv_scr_load(ui_MotorConfigScreen);
  
}

// Go to DriveStatusScreen
void DriveStatus_evt_handler(lv_event_t * e)
{
  lv_scr_load(ui_DriveStatusScreen);
}

static void delayed_ip_config_task(lv_timer_t * timer) {
  // This runs on the next LVGL tick after the UI has updated
  String ipToSend = "";
  ipToSend += (String)newIpAddress[0] + "," + (String)newIpAddress[1] + ","+ (String)newIpAddress[2] + ","+ (String)newIpAddress[3];
  configNetwork(ipToSend);
  getDriveInfo();
  lv_obj_add_flag(ui_SpinnerIPConfig, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(ui_SendButtonIPConfigLabel, "Send");
  // Delete the one-shot timer
  lv_timer_del(timer);
}

// Send IP Address to drive and open network
void SendIP_evt_handler(lv_event_t * e)
{
  lv_obj_clear_flag(ui_SpinnerIPConfig, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(ui_SendButtonIPConfigLabel, "Sending...");
  lv_timer_create(delayed_ip_config_task, 10, NULL);
}


void setIpDigits()
{
  int selIdx1 = lv_roller_get_selected(ui_IPDigit1);
  int selIdx2 = lv_roller_get_selected(ui_IPDigit2);
  int selIdx3 = lv_roller_get_selected(ui_IPDigit3);
  char newDigit1 = (char)(selIdx1 + '0');
  char newDigit2 = (char)(selIdx2 + '0');
  char newDigit3 = (char)(selIdx3 + '0');
  if(selIdx1 == 10)
  {
    newDigit1 = '-';
  }
  if(selIdx2 == 10)
  {
    newDigit2 = '-';
  }
  if(selIdx3 == 10)
  {
    newDigit3 = '-';
  }
  newIpAddress[currentField][0] = newDigit1;
  newIpAddress[currentField][1] = newDigit2;
  newIpAddress[currentField][2] = newDigit3;
  setIpLabel();
}

// Set IP Digit 1 based on spinner
void SetIPDigit1_evt_handler(lv_event_t * e)
{
  setIpDigits();
}

// Set IP Digit 2 based on spinner
void SetIPDigit2_evt_handler(lv_event_t * e)
{
  setIpDigits();
}

// Set IP Digit 3 based on spinner
void SetIPDigit3_evt_handler(lv_event_t * e)
{
  setIpDigits();
}

// Change IP Field to previous
void IPPrevField_evt_handler(lv_event_t * e)
{
  currentField --;
  if(currentField < 0)
  {
    currentField += 4;
  }
  String areaIpStr = newIpAddress[currentField];
  areaIpStr.replace("-","0");
  int areaIp = areaIpStr.toInt();
  // replace leading zeros with "-" (index 10)
  int d1 = areaIp / 100 == 0? 10: areaIp / 100; // get 100s place
  int d2 = (areaIp % 100) / 10; // get 10s place
  int d3 = areaIp % 10; // get 1s place

  // set rollers
  lv_roller_set_selected(ui_IPDigit1, d1, LV_ANIM_ON);
  lv_roller_set_selected(ui_IPDigit2, d2, LV_ANIM_ON);
  lv_roller_set_selected(ui_IPDigit3, d3, LV_ANIM_ON);
  setIpDigits();
  setIpLabel();
}

// Change IP Field to next
void IPNextField_evt_handler(lv_event_t * e)
{
  currentField++;
  if(currentField > 3)
  {
    currentField -= 4;
  }
  String areaIpStr = newIpAddress[currentField];
  areaIpStr.replace("-","0");
  int areaIp = areaIpStr.toInt();
  // replace leading zeros with "-" (index 10)
  int d1 = areaIp / 100 == 0? 10: areaIp / 100; // get 100s place
  int d2 = (areaIp % 100) / 10; // get 10s place
  int d3 = areaIp % 10; // get 1s place
  // set rollers
  lv_roller_set_selected(ui_IPDigit1, d1, LV_ANIM_ON);
  lv_roller_set_selected(ui_IPDigit2, d2, LV_ANIM_ON);
  lv_roller_set_selected(ui_IPDigit3, d3, LV_ANIM_ON);
  setIpDigits();
  setIpLabel();
}

static void delayed_motor_config_task(lv_timer_t * timer) {
  // This runs on the next LVGL tick after the UI has updated
  configMotor(selectedMotorConfig);
  
  // Reset UI
  lv_label_set_text(ui_SendButtonMotorConfigLabel, "Send Configuration");
  lv_obj_add_flag(ui_SpinnerMotorConfig, LV_OBJ_FLAG_HIDDEN);
  
  // Delete the one-shot timer
  lv_timer_del(timer);
  sentConfig = false;
}

// Send Motor Configuration to drive
void SendMotorConfig_evt_handler(lv_event_t * e)
{
  if(sentConfig)
  {
    return;
  }
  sentConfig = true;
  lv_obj_clear_flag(ui_SpinnerMotorConfig, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(ui_SendButtonMotorConfigLabel, "Sending...");
  lv_timer_create(delayed_motor_config_task, 10, NULL);
  
}

// Set selected motor config based on user input
void SetMotorConfig_evt_handler(lv_event_t * e)
{
  char buf[64];
  lv_dropdown_get_selected_str(ui_MotorConfigSelect, buf, sizeof(buf));
  selectedMotorConfig = buf;
}

static void delayed_enable_task(lv_timer_t * timer)
{
  sendCommand("NTFEN1");
  String res = receiveData().substring(0,6);
  if(res == "NTFEN1")
  {
    logInfo("Network Enable Success");
  }
  else
  {
    logInfo("ERR: Unable to verify NTFEN success, please try again");
  }

  lv_label_set_text(ui_EnaNetworkButtonStatusLabel, "Enable\nNetwork");
  lv_obj_add_flag(ui_SpinnerStatus, LV_OBJ_FLAG_HIDDEN);
  
  // Delete the one-shot timer
  lv_timer_del(timer);
}

// Enable drive network connections
void EnableNetwork_evt_handler(lv_event_t * e)
{
  lv_obj_clear_flag(ui_SpinnerStatus, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(ui_EnaNetworkButtonStatusLabel, "Enabling...");
  lv_timer_create(delayed_enable_task, 10, NULL);
}

static void delayed_factory_reset_task(lv_timer_t * timer) 
{
  // This runs on the next LVGL tick after the UI has updated
  sendCommand("RFS");
  logInfo("Factory Reset sent, re-establishing connecting...");
  establishContact();
  sendCommand("NTFEN1");
  String res = receiveData().substring(0,6);
  if(res == "NTFEN1")
  {
    logInfo("Network Enable Success");
  }
  else
  {
    logInfo("ERR: Unable to verify NTFEN success, please try again");
  }
  
  // Reset UI
  lv_label_set_text(ui_FactoryResetButtonStatusLabel, "Factory\nReset");
  lv_obj_add_flag(ui_SpinnerStatus, LV_OBJ_FLAG_HIDDEN);
  
  // Delete the one-shot timer
  lv_timer_del(timer);
}

// Factory Reset the drive and enable network connections
void FactoryReset_evt_handler(lv_event_t * e)
{
  lv_obj_clear_flag(ui_SpinnerStatus, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(ui_FactoryResetButtonStatusLabel, "Resetting...");
  lv_timer_create(delayed_factory_reset_task, 10, NULL);
}
