/*
  xdrv_90_zr31.ino - Analog ZR31 Output Support for ESP32

    This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  --------------------------------------------------------------------------------------------
  Version yyyymmdd  Action    Description
  --------------------------------------------------------------------------------------------
  
*/

#ifdef USE_ZR31
/*********************************************************************************************\
 * ZR31 Fan control
\*********************************************************************************************/

#define XDRV_90            90

/*********************************************************************************************\
 * constants
\*********************************************************************************************/
#define D_CMND_ZR31            "ZR31"

#define D_GAIN_ZR31            3       // Gain of amplifier to be included
#define D_MAX_VOLTAGE_ZR31     3.188   // Max Voltage of DAC Pin
#define D_MANUAL_VOLTAGE_ZR31  0       // Voltage to switch off
#define D_OFF_VOLTAGE_Z31      0.5     // Voltage to switch off
#define D_LOW_REVERSE_Z31      1.5     // Low Voltage of Reverse (safety margin applied)
#define D_HIGH_REVERSE_Z31     5.2     // High Voltage of Reverse (safety margin applied)
#define D_LOW_FLOW_Z31         6.1     // Low Voltage of Flow (safety margin applied)
#define D_HIGH_FLOW_Z31        9.2     // High Voltage of Flow (safety margin applied)

const char kZR31_Commands[] PROGMEM             = "SETVOLTLEVEL|SETVOLTAGE|SWITCHOFF|REVERSE|FLOW|MANUAL";
const char S_JSON_ZR31_COMMAND_NVALUE[] PROGMEM = "{\"" D_CMND_ZR31 "%s\":%d}";

const char HTTP_ZR31_FUNCTION[] PROGMEM = "{s}"     D_JSON_FUNCTION            "{m}%s{e}";
const char HTTP_ZR31_LEVEL[] PROGMEM    = "{s}"     D_JSON_LEVEL               "{m}%d{e}";

/*********************************************************************************************\
 * enumerationsines
\*********************************************************************************************/

 enum ZR31_Commands {                                 // commands useable in console or rules
  CMND_ZR31_SETVOLTLEVEL,                             // set voltage 0 ... 255
  CMND_ZR31_SETVOLTAGE,                               // set voltage 0...MAX
  CMND_ZR31_SWITCHOFF,                                // set voltage 0...MAX
  CMND_ZR31_REVERSE,                                  // Reverse 0...10
  CMND_ZR31_FLOW,                                     // Forward Flow 0...10
  CMND_ZR31_MANUAL                                    // Switch to manual                                  
 };

struct ZR31_STATE{
  uint8_t function              = 0;                // 0 = manual, 1 = off, 2 = flow, 3 = reverse
  uint8_t level                 = 0;                // 0...10
  float   voltage               = 0;                // voltage after gain
};

ZR31_STATE ZR31State;



/*********************************************************************************************\
 * init ZR31
\*********************************************************************************************/

void ZR31Init(void) {
  ZR31SetVoltage(D_MANUAL_VOLTAGE_ZR31);
  ZR31State.function = 0;
  return;
}


/*********************************************************************************************\
 * check the DAC commands
\*********************************************************************************************/

bool ZR31Cmd(void) {
  char command[CMDSZ];
  bool serviced = true;
  uint8_t disp_len = strlen(D_CMND_ZR31);

  if (!strncasecmp_P(XdrvMailbox.topic, PSTR(D_CMND_ZR31), disp_len)) {  // prefix
    int command_code = GetCommandCode(command, sizeof(command), XdrvMailbox.topic + disp_len, kZR31_Commands);

    switch (command_code) {
      case CMND_ZR31_SETVOLTLEVEL:
        if(XdrvMailbox.payload > 255 || XdrvMailbox.payload < 0 ){
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, -1);  //error
        }else{
          ZR31SetVoltLevel(XdrvMailbox.payload);
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, XdrvMailbox.payload);
        }
      break;
      case CMND_ZR31_SETVOLTAGE:
        if(XdrvMailbox.payload > ( D_MAX_VOLTAGE_ZR31 * D_GAIN_ZR31 )){
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, -1);  //error
        }else{
          ZR31SetVoltage(XdrvMailbox.payload);
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, XdrvMailbox.payload);
        }
      break;
      case CMND_ZR31_SWITCHOFF:
        ZR31SetVoltage(D_OFF_VOLTAGE_Z31);
        Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, 0);
        ZR31State.function = 1;
        ZR31State.level = -1;
      break;
      case CMND_ZR31_MANUAL:
        ZR31SetVoltage(D_MANUAL_VOLTAGE_ZR31);
        Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, 1);
        ZR31State.function = 0;
        ZR31State.level = -1;
      break;
      case CMND_ZR31_REVERSE:
        if(XdrvMailbox.payload < 0 || XdrvMailbox.payload > 10){
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, -1);
        }else{
          ZR31SetReverseHeat(XdrvMailbox.payload);
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, XdrvMailbox.payload);
          ZR31State.function = 3;
        }
      break;
      case CMND_ZR31_FLOW:
        if(XdrvMailbox.payload < 0 || XdrvMailbox.payload > 10){
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, -1);
        }else{
          ZR31SetFlow(XdrvMailbox.payload);
          Response_P(S_JSON_ZR31_COMMAND_NVALUE, command, XdrvMailbox.payload);
          ZR31State.function = 2;
        }
      break;

      default:
    	  // else for Unknown command
    	  serviced = false;
    	break;
    }
  } else {
    return false;
  }
  return serviced;
}



/*********************************************************************************************\
 * set volt level and voltage
\*********************************************************************************************/
void ZR31SetVoltLevel(uint16_t voltLevel){
    dacWrite(Pin(GPIO_DAC), voltLevel);
}

void ZR31SetVoltage(float ZR31voltage){
  float ZR31OutVoltage;
  float voltLevel;

  ZR31OutVoltage = ZR31voltage / (float) D_GAIN_ZR31;
  voltLevel = ( ZR31OutVoltage  / (float) D_MAX_VOLTAGE_ZR31 ) * 255;

  ZR31SetVoltLevel((uint16_t) voltLevel);
  ZR31State.voltage = ZR31voltage;
}

/*********************************************************************************************\
 * set Return heat and air flow
\*********************************************************************************************/
void ZR31SetReverseHeat(uint16_t Z31Level){
  float ZR31OutVoltage;
  ZR31OutVoltage = D_LOW_REVERSE_Z31 + ( (float) ( D_HIGH_REVERSE_Z31 - D_LOW_REVERSE_Z31 ) / (float) 10.0 ) * Z31Level;
  ZR31SetVoltage( ZR31OutVoltage );
  ZR31State.function = 3;
  ZR31State.level = Z31Level;
}

void ZR31SetFlow(uint16_t Z31Level){
  float ZR31OutVoltage;
  ZR31OutVoltage = D_LOW_FLOW_Z31 + ( (float) ( D_HIGH_FLOW_Z31 - D_LOW_FLOW_Z31 ) / (float) 10.0 ) * Z31Level;
  ZR31SetVoltage( ZR31OutVoltage );
  ZR31State.function = 2;
  ZR31State.level = Z31Level;
}

/*********************************************************************************************\
 * Results / States
\*********************************************************************************************/
void ZR31Show(bool json)
{

  char zr31Status[8];
  switch(ZR31State.function){
    case 0:
      sprintf (zr31Status,"manual");
      break;
    case 1:
      sprintf (zr31Status,"off");
      break;
    case 2:
      sprintf (zr31Status,"flow");
      break;
    case 3:
      sprintf (zr31Status,"reverse");
      break;
  }

  char voltage_chr[FLOATSZ];
  dtostrfd(ZR31State.voltage, Settings.flag2.voltage_resolution, voltage_chr);

  if (json) {
    ResponseAppend_P(PSTR(",\"ZR31\":{\"" D_JSON_FUNCTION "\":\"%s\",\"" D_JSON_LEVEL "\":\"%d\",\"" D_JSON_VOLTAGE "\":\"%s\"}"), zr31Status, ZR31State.level, voltage_chr );
  }else{
    #ifdef USE_WEBSERVER  // USE_WEBSERVER

    WSContentSend_PD(HTTP_ZR31_FUNCTION, zr31Status);
    WSContentSend_PD(HTTP_SNS_VOLTAGE, voltage_chr);
    WSContentSend_PD(HTTP_ZR31_LEVEL, ZR31State.level);
    #endif  // USE_WEBSERVER
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv90(uint8_t function)
{
  bool result = false;

  if (PinUsed(GPIO_DAC)) {
    switch (function) {
      case FUNC_PRE_INIT:
        ZR31Init();                              // init and start communication
        break;
      case FUNC_COMMAND:
        result = ZR31Cmd();                      // return result from mp3 player command
        break;
      case FUNC_JSON_APPEND:                     // return current state
        ZR31Show(1);
        break;
      #ifdef USE_WEBSERVER  // USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        ZR31Show(0);
        break;
      #endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_ZR31
