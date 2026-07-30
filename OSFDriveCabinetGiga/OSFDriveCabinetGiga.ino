#include "Configs.h"
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include "ui.h"
// #include <Arduino_USBHostMbed5.h>    // USB Host for GIGA R1
// #include <FATFileSystem.h>           // FAT file system (mbed)
#include <stdio.h> 
// #include "mbed_error.h"
// #include "mbed.h"
// #include "mbed_mem_trace.h"

// SETUP GIGA
Arduino_H7_Video Display(800, 480, GigaDisplayShield); 
Arduino_GigaDisplayTouch Touch;
GDTpoint_t touchPoints[5];

// // Setup USB for images
// USBHostMSD usb;
// mbed::FATFileSystem fs("usb");

// SLEEP
bool isSleeping = false;
const int SLEEP_DELAY_MS = 30000; // Sleep after 30 seconds
unsigned long lastActivityTime = 0;

// CONNECTION PINGING
const int CHECK_CONNECTION_MS = 20000; // Check connection every 10 seconds
const int TIMEOUT_TIME_MS = 1000; // If we dont get a response in 1 seconds, we have timed out
unsigned long lastConnectedTime = 0;
unsigned long connectionFirstCheck = 0;
unsigned long connectionLastCheck = 0;

// COMMUNICATION INTERLOCK
bool communicating = false;
String pendingCommand = "";

// VAR USED IN MULTIPLE SCREENS
String driveName = "";
bool connected = false;
String motorType = "";
String ipAddress = "";
const int logSize = 10;
int curLog = 0;
String logs[logSize];
//static unsigned long lastBlink = 0;


// VAR USED IN IPCONFIG
char newIpAddress[][4] = {"---","---","---","---"};
int currentField = 3;

// VAR USED IN MOTOR CONFIG
String selectedMotorConfig = "MPP115 3C1E";
bool sentConfig = false;

// VAR USED IN DRIVE STATUS
String driveStatusPrint = "";


// Setup Variables, UI, and Connection
void setup() {
  Serial.begin(9600);
  delay(3000);

  Display.begin();
  Touch.begin();
  ui_init();
  //setupUIBackgrounds();
  lv_scr_load(ui_StartupScreen);
  
  // Setup UI Elements
  lv_label_set_text(ui_LoadingScreenText, "Loading UI Elements");
  lv_timer_handler();
  setupEventHandlers();
  setupUIOptions();
  
  lv_label_set_recolor(ui_IPDisplay, true);

  // Setup Serial communication
  
  lv_label_set_text(ui_LoadingScreenText, "Starting Serial Communication");
  lv_timer_handler();
  // Begin Serial to communicate with pc, Serial2(blue-19tx, yellow-18rx) to communicate with driver
  //Serial.begin(9600);
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
  // Load Home Screen 
  lv_scr_load(ui_HomeScreen);
  lv_timer_handler();
  unsigned long lastActivityTime = millis();

  // TEMP TESTING SETUP FOR NO DRIVE
  // int lastAreaIp = ipAddress.substring(ipAddress.lastIndexOf(".")+1).toInt();
  // currentField = 3;
  // setIpLabel();
  // //Serial.println(ipAddress);
  // //Serial.println(lastAreaIp);
  // // replace leading zeros with "-" (index 10)
  // int d1 = lastAreaIp / 100 == 0? 10: lastAreaIp / 100; // get 100s place
  // int d2 = (lastAreaIp % 100) / 10; // get 10s place
  // int d3 = lastAreaIp % 10; // get 1s place
  // // set rollers
  // lv_roller_set_selected(ui_IPDigit1, d1, LV_ANIM_OFF);
  // lv_roller_set_selected(ui_IPDigit2, d2, LV_ANIM_OFF);
  // lv_roller_set_selected(ui_IPDigit3, d3, LV_ANIM_OFF);
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
  //lv_obj_add_flag(ui_SpinnerMotorConfig, LV_OBJ_FLAG_HIDDEN);
}

// Initiate handshake with GV6K
void establishContact() {
  String response = "";
  while(response != "ECHO,*ECHO1")
  {
    lv_timer_handler();
    // Verify connection with GV6K
    Serial.println("connecting...");
    while (Serial2.available() <= 0) {
      lv_timer_handler();
      Serial2.println("echo");
      delay(300);
    }
    response = receiveData();
    //Serial.println(response == "ECHO,*ECHO1");
  }
  // Once connected read the serial buffer to discard handshake data
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
  if(connectionFirstCheck == 0)
  {
    // if(communicating)
    // {
    //   return;
    // }
    // first check of this cycle
    connectionFirstCheck = millis();
    if (connected == false)
    {
      lv_label_set_text(ui_StatusLabelHome, "Not Connected");
      lv_label_set_text(ui_StatusLabelIPConfig, "Not Connected");
      lv_label_set_text(ui_StatusLabelMotorConfig, "Not Connected");
      lv_label_set_text(ui_StatusLabelStatus, "Not Connected");
    }
    else
    {
      lv_label_set_text(ui_StatusLabelHome, "Connecting...");
      lv_label_set_text(ui_StatusLabelIPConfig, "Connecting...");
      lv_label_set_text(ui_StatusLabelMotorConfig, "Connecting...");
      lv_label_set_text(ui_StatusLabelStatus, "Connecting...");
    }
    //communicating = true;
    Serial2.println("echo");
    connectionLastCheck = millis();
  }
  //Read Response 
  if(Serial2.available() > 0)
  {
    String response = receiveData();
    if(response == "ECHO,*ECHO1")
    {
      // SUCCESS
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
  // move loop to loop function to reduce blocking
  if(millis() - connectionLastCheck > 300)
  {
    Serial2.println("echo");
    connectionLastCheck = millis();
  }
  if(millis() - connectionFirstCheck > TIMEOUT_TIME_MS)
  {
    //lastConnectedTime = millis();
    connected = false;
    communicating = false;
    connectionFirstCheck = 0;
    lv_label_set_text(ui_StatusLabelHome, "Not Connected");
    lv_label_set_text(ui_StatusLabelIPConfig, "Not Connected");
    lv_label_set_text(ui_StatusLabelMotorConfig, "Not Connected");
    lv_label_set_text(ui_StatusLabelStatus, "Not Connected");
    // TODO? reset home screen labels?
    logInfo("Connection Timeout");
    return;
  }
  
}

// Read drive type, motor type, ip config, and other required details
// Call everytime we enter motor config screen?
void getDriveInfo()
{
  sendCommand("TREV:TNT:DMTR:CMDDIR:SGINTE:LJRAT:LDAMP");
  String setupData = receiveSetupData();
  String TREV = setupData.substring(setupData.indexOf("TREV"), setupData.indexOf(","));
  String TNT = setupData.substring(setupData.indexOf("TNT"), setupData.indexOf(", DMTR"));
  String DMTR = setupData.substring(setupData.indexOf("*DMTR")+5, setupData.indexOf(", CMDDIR"));
  String CMDDIR = setupData.substring(setupData.indexOf("*CMDDIR")+7,setupData.indexOf(", SGINTE"));
  String SGINTE = setupData.substring(setupData.indexOf("*SGINTE")+7,setupData.indexOf(", LJRAT"));
  String LJRAT = setupData.substring(setupData.indexOf("*LJRAT")+6,setupData.indexOf(", LDAMP"));
  String LDAMP = setupData.substring(setupData.indexOf("*LDAMP")+6,setupData.indexOf(", ,"));
  String temp = "";
  TREV.trim();
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
  // firmware
  driveStatusPrint += "Firmware Version: "+TREV.substring(TREV.indexOf(" ", GVIdx)+1,TREV.lastIndexOf(" "))+"\n";
  // Flash Boot
  driveStatusPrint += "Flash Boot Revision: "+TREV.substring(TREV.lastIndexOf(" ")+1)+"\n";


  // TNT for netstats
  // process ip
  temp = TNT.substring(TNT.indexOf("*GEM6K IP address:")+19, TNT.indexOf(",",TNT.indexOf("*GEM6K IP address:")));
  temp.substring(0, temp.indexOf(".")).toCharArray(newIpAddress[0], sizeof(newIpAddress[0]));
  temp.substring(temp.indexOf(".")+1, temp.indexOf(".",4)).toCharArray(newIpAddress[1], sizeof(newIpAddress[1]));
  temp.substring(temp.indexOf(".",4)+1, temp.lastIndexOf(".")).toCharArray(newIpAddress[2], sizeof(newIpAddress[2]));
  temp.substring(temp.lastIndexOf(".")+1).toCharArray(newIpAddress[3], sizeof(newIpAddress[3]));
  int lastAreaIp = temp.substring(temp.lastIndexOf(".")+1).toInt();
  ipAddress = temp;
  TNT.replace("*", "");
  TNT.replace(',', '\n');
  driveStatusPrint += TNT;
  driveStatusPrint += "\n";

  // DMTR tells motor config
  String motorTypeTemp = motorType;
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
  //driveStatusPrint += "Current Motor: "+motorTypeTemp+"\n";

  // cmddir, sginte, ljrat, ldamp
  driveStatusPrint += "CMDDIR: "+CMDDIR+"\nSGINTE: "+SGINTE+"\nLJRAT: "+LJRAT+"\nLDAMP: "+LDAMP;
  Serial.println(driveStatusPrint);
  Serial.println(LDAMP);
  // UPDATE UI
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
  //Serial.println("Drive status labels NOT set");
}

void loop() {
  //print_memory_info();
  lv_timer_handler();
  
  // Wake up on touch
  if (Touch.getTouchPoints(touchPoints) > 0) {
    //Serial.println("Touch");
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

  if (millis() - lastConnectedTime > CHECK_CONNECTION_MS)
  {
    // start connection check. send message and check for response once per loop until timeout
    checkConnection(); // start
    
  }

  if(pendingCommand != "" && !communicating)
  {
    //sendCommand(pendingCommand);
  }
}

// Write given string to display logs and update UIs
void logInfo(String log)
{
  Serial.println(log);
  String logText = "";
  // wrap around
  logs[curLog] = log;
  curLog++;
  if(curLog >= logSize)
  {
    curLog = 0;
  }
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

// Sends command with value over serial to drive
void sendCommand(Command cmd)
{
  if(communicating && pendingCommand == "")
  {
    pendingCommand = cmd.ToString();
    logInfo("communication in progress, adding to pending cue: " + pendingCommand);
    return;
  }
  // Clear previous buffer
  Serial2.readString();
  // send command to drive as well as to serial monitor
  communicating = true;
  Serial.println(cmd.ToString());
  Serial2.println(cmd.ToString());
  logInfo("SENT: " + cmd.ToString());
  communicating = false;
  pendingCommand = "";
}

// Sends command with no value over serial
void sendCommand(String cmd)
{
  if(communicating && pendingCommand == "")
  {
    pendingCommand = cmd;
    logInfo("communication in progress, adding to pending cue: " + pendingCommand);
    //return;
  }
  // Clear previous buffer
  Serial2.readString();
  communicating = true;
  // send command to drive as well as to serial monitor
  Serial2.println(cmd);
  logInfo("SENT: " + cmd);
  communicating = false;
  pendingCommand = "";
}

String receiveSetupData()
{
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
  // Allow multiple iterations to recieve response (25*100 = 2.5 seconds)
  while (!end && count <= 300)
  {
    
    if(millis() - lastCheck < 25)
    {
      continue;
    }
    lastCheck = millis();
    // "*line 1\r\nline2\r\r\n"
    // "*Ldamp0.000\r\n>\n\r\n"
    while (Serial2.available() > 0)
    {
      lv_timer_handler();
      char curChar = Serial2.read();
      //Serial.print("CHAR: "+(String)curChar+"\t SOL:"+(String)startOfLine+"\tfirstR:"+(String)firstR+"\tend:"+(String)end+"\tcount"+(String)count+"\n");
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
      switch(curChar) 
      {
        case '\r':
          break;
        case '\n':
          if(firstN)
          {
            end = true;
          }
          else
          {
            result += ',';
            startOfLine = false;
            firstN = true;
          }
          break;
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

String receiveData()
{
  
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
  // Allow multiple iterations to recieve response (25*100 = 2.5 seconds)
  while (!end && count <= 300)
  {
    
    if(millis() - lastCheck < 25)
    {
      continue;
    }
    lastCheck = millis();
    // "*line 1\r\nline2\r\r\n"
    while (Serial2.available() > 0)
    {
      char curChar = Serial2.read();
      //Serial.print("CHAR: "+(String)curChar+"\t SOL:"+(String)startOfLine+"\tfirstR:"+(String)firstR+"\tend:"+(String)end+"\tcount"+(String)count+"\n");
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
      switch(curChar) 
      {
        case '\r':
          if (!firstR)
          {
            // first return
            firstR = true;
          }
          else
          {
            // second return in a row, we are at EOT
            end = true;
            firstR = false;
          }
          break;
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

bool sendConfig(Command config[], int configSize)
{
  if(communicating)
  {
    logInfo("Mid communication, please try again");
    //return false;
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

  //Serial.println(motorType);
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
    //logInfo("[ERR] Motor type not recognize: " + motorType);
    success = false;
  }
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
  // ip must be separated by commas; 192,102,....
  // Set IP
  sendCommand("NTADDR"+ip);
  String addrResp = receiveData();
  // Enable Ethernet
  sendCommand("NTFEN1");
  String ntfenResp = receiveData();
  sendCommand("RESET");
  receiveData();
  establishContact();
  // TODO Check that addr and ntfen response are correct
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
  //Serial.println("IP DISPLAY: "+ipDisplay);
  char ipBuf[30];
  ipDisplay.toCharArray(ipBuf, sizeof(ipBuf));
  lv_label_set_text(ui_IPDisplay, ipBuf);
}

// Go to HomeScreen
void Home_evt_handler(lv_event_t * e)
{
  //getDriveInfo();
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
  //getDriveInfo();
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
  //Serial.println("IP NEXT: "+areaIpStr);
  areaIpStr.replace("-","0");
  //Serial.println("IP NEXT: "+areaIpStr);
  int areaIp = areaIpStr.toInt();
  // replace leading zeros with "-" (index 10)
  int d1 = areaIp / 100 == 0? 10: areaIp / 100; // get 100s place
  int d2 = (areaIp % 100) / 10; // get 10s place
  int d3 = areaIp % 10; // get 1s place
  //Serial.println((String)d1+" "+(String)d2+" "+(String)d3);
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
