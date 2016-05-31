/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// No include guards here, this file may be included many times in different namespaces
// i.e. BACKUP RAM Backup/Restore functions

#include <inttypes.h>
#include "dataconstants.h"

#if defined(CPUARM)
  #define ARM_FIELD(x)                 x;
  #define AVR_FIELD(x)
#else
  #define ARM_FIELD(x)
  #define AVR_FIELD(x)                 x;
#endif

#if defined(PCBSTD)
  #define N_PCBSTD_FIELD(x)
#else
  #define N_PCBSTD_FIELD(x) x;
#endif

#if defined(PCBTARANIS)
  #define N_TARANIS_FIELD(x)
  #define TARANIS_FIELD(x) x;
#else
  #define N_TARANIS_FIELD(x) x;
  #define TARANIS_FIELD(x)
#endif

#if defined(PCBTARANIS) && defined(REV9E)
  #define TARANIS_REV9E_FIELD(x)       x;
#else
  #define TARANIS_REV9E_FIELD(x)
#endif

#if defined(PCBHORUS)
  #define N_HORUS_FIELD(x)
  #define HORUS_FIELD(x) x;
#else
  #define N_HORUS_FIELD(x) x;
  #define HORUS_FIELD(x)
#endif

#if defined(BACKUP)
  #define NOBACKUP(...)
#else
  #define NOBACKUP(...)                __VA_ARGS__
#endif

#if __GNUC__
  #define PACK( __Declaration__ )      __Declaration__ __attribute__((__packed__))
#else
  #define PACK( __Declaration__ )      __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif

#if defined(PCBTARANIS) || defined(PCBHORUS)
typedef uint16_t source_t;
#else
typedef uint8_t source_t;
#endif

/*
 * Mixer structure
 */

#if defined(VIRTUALINPUTS)
PACK(struct CurveRef {
  uint8_t type;
  int8_t  value;
});

PACK(struct MixData {
  int16_t  weight:11;       // GV1=-1024, -GV1=1023
  uint16_t destCh:5;
  uint16_t srcRaw:10;       // srcRaw=0 means not used
  uint16_t carryTrim:1;
  uint16_t mixWarn:2;       // mixer warning
  uint16_t mltpx:2;         // multiplex method: 0 means +=, 1 means *=, 2 means :=
  uint16_t spare:1;
  int32_t  offset:14;
  int32_t  swtch:9;
  uint32_t flightModes:9;
  CurveRef curve;
  uint8_t  delayUp;
  uint8_t  delayDown;
  uint8_t  speedUp;
  uint8_t  speedDown;
  NOBACKUP(char name[LEN_EXPOMIX_NAME]);
});
#elif defined(CPUARM)
PACK(struct MixData {
  uint8_t  destCh:5;
  uint8_t  mixWarn:3;         // mixer warning
  uint16_t flightModes:9;
  uint16_t curveMode:1;
  uint16_t noExpo:1;
  int16_t  carryTrim:3;
  uint16_t mltpx:2;           // multiplex method: 0 means +=, 1 means *=, 2 means :=
  int16_t  weight;
  int8_t   swtch;
  int8_t   curveParam;
  uint8_t  delayUp;
  uint8_t  delayDown;
  uint8_t  speedUp;
  uint8_t  speedDown;
  uint8_t  srcRaw;
  int16_t  offset;
  NOBACKUP(char name[LEN_EXPOMIX_NAME]);
});
#elif defined(CPUM2560) || defined(CPUM2561)
PACK(struct MixData {
  uint8_t destCh:4;          // 0, 1..NUM_CHNOUT
  uint8_t curveMode:1;       // O=curve, 1=differential
  uint8_t noExpo:1;
  uint8_t weightMode:1;
  uint8_t offsetMode:1;
  uint8_t srcRaw;
  int8_t  weight;
  int8_t  swtch;
  uint8_t flightModes;
  uint8_t mltpx:2;           // multiplex method: 0 means +=, 1 means *=, 2 means :=
  int8_t  carryTrim:3;
  uint8_t mixWarn:2;         // mixer warning
  uint8_t spare:1;
  uint8_t delayUp:4;
  uint8_t delayDown:4;
  uint8_t speedUp:4;
  uint8_t speedDown:4;
  int8_t  curveParam;
  int8_t  offset;
});
#else
PACK(struct MixData {
  uint8_t destCh:4;          // 0, 1..NUM_CHNOUT
  uint8_t curveMode:1;       // O=curve, 1=differential
  uint8_t noExpo:1;
  uint8_t weightMode:1;
  uint8_t offsetMode:1;
  int8_t  weight;
  int8_t  swtch:6;
  uint8_t mltpx:2;           // multiplex method: 0 means +=, 1 means *=, 2 means :=
  uint8_t flightModes:5;
  int8_t  carryTrim:3;
  uint8_t srcRaw:6;
  uint8_t mixWarn:2;         // mixer warning
  uint8_t delayUp:4;
  uint8_t delayDown:4;
  uint8_t speedUp:4;
  uint8_t speedDown:4;
  int8_t  curveParam;
  int8_t  offset;
});
#endif

/*
 * Expo/Input structure
 */

#if defined(VIRTUALINPUTS)
PACK(struct ExpoData {
  uint16_t mode:2;
  uint16_t scale:14;
  uint16_t srcRaw:10;
  int16_t  carryTrim:6;
  uint32_t chn:5;
  int32_t  swtch:9;
  uint32_t flightModes:9;
  int32_t  weight:8;
  int32_t  spare:1;
  NOBACKUP(char name[LEN_EXPOMIX_NAME]);
  int8_t   offset;
  CurveRef curve;
});
#elif defined(CPUARM)
PACK(struct ExpoData {
  uint16_t mode:2;         // 0=end, 1=pos, 2=neg, 3=both
  uint16_t chn:3;
  uint16_t curveMode:2;
  uint16_t flightModes:9;
  int8_t   swtch;
  int8_t   weight;
  NOBACKUP(char name[LEN_EXPOMIX_NAME]);
  int8_t   curveParam;
});
#elif defined(CPUM2560) || defined(CPUM2561)
PACK(struct ExpoData {
  uint8_t mode:2;         // 0=end, 1=pos, 2=neg, 3=both
  uint8_t chn:2;
  uint8_t curveMode:1;
  uint8_t spare:3;
  uint8_t flightModes;
  int8_t  swtch;
  uint8_t weight;
  int8_t  curveParam;
});
#else
PACK(struct ExpoData {
  uint8_t mode:2;         // 0=end, 1=pos, 2=neg, 3=both
  int8_t  swtch:6;
  uint8_t chn:2;
  uint8_t flightModes:5;
  uint8_t curveMode:1;
  uint8_t weight;         // One spare bit here (used for GVARS)
  int8_t  curveParam;
});
#endif

/*
 * Limit structure
 */

#if defined(VIRTUALINPUTS)
PACK(struct LimitData {
  int32_t min:11;
  int32_t max:11;
  int32_t ppmCenter:10;
  int16_t offset:11;
  uint16_t symetrical:1;
  uint16_t revert:1;
  uint16_t spare:3;
  int8_t curve;
  NOBACKUP(char name[LEN_CHANNEL_NAME]);
});
#else
PACK(struct LimitData {
  int8_t min;
  int8_t max;
  int8_t  ppmCenter;
  int16_t offset:14;
  uint16_t symetrical:1;
  uint16_t revert:1;
});
#endif

/*
 * LogicalSwitch structure
 */

#if defined(CPUARM)
PACK(struct LogicalSwitchData {
  uint8_t  func;
  int32_t  v1:10;
  int32_t  v3:10;
  int32_t  andsw:9;      // TODO rename to xswtch
  uint32_t andswtype:1;  // TODO rename to xswtchType (AND / OR)
  uint32_t spare:2;      // anything else needed?
  int16_t  v2;
  uint8_t  delay;
  uint8_t  duration;
});
#else
PACK(struct LogicalSwitchData {
  int8_t  v1; //input
  int8_t  v2; //offset
  uint8_t func:4;
  uint8_t andsw:4;
});
#endif

/*
 * SpecialFunction structure
 */

#if defined(CPUARM)

#if defined(PCBTARANIS)
  #define CFN_SPARE_TYPE               int32_t
#else
  #define CFN_SPARE_TYPE               int16_t
#endif

PACK(struct CustomFunctionData {
  int16_t  swtch:9;
  uint16_t func:7;
  PACK(union {
    NOBACKUP(PACK(struct {
      char name[LEN_CFN_NAME];
    }) play);

    PACK(struct {
      int16_t val;
      uint8_t mode;
      uint8_t param;
      NOBACKUP(CFN_SPARE_TYPE spare);
    }) all;

    PACK(struct {
      int32_t val1;
      NOBACKUP(CFN_SPARE_TYPE val2);
    }) clear;
  });
  uint8_t active;
});
#elif defined(CPUM2560)
PACK(struct CustomFunctionData {
  int8_t  swtch;
  uint8_t func;
  uint8_t mode:2;
  uint8_t param:4;
  uint8_t active:1;
  uint8_t spare:1;
  uint8_t value;
});
#else
PACK(struct CustomFunctionData {
  PACK(union {
    PACK(struct {
      int16_t   swtch:6;
      uint16_t  func:4;
      uint16_t  mode:2;
      uint16_t  param:3;
      uint16_t  active:1;
    }) gvar;

    PACK(struct {
      int16_t   swtch:6;
      uint16_t  func:4;
      uint16_t  param:4;
      uint16_t  spare:1;
      uint16_t  active:1;
    }) all;
  });
  uint8_t value;
});
#endif

/*
 * FlightMode structure
 */

 #if defined(VIRTUALINPUTS)
 PACK(struct trim_t {
   int16_t  value:11;
   uint16_t mode:5;
 });
 #else
 typedef int16_t trim_t;
 #endif

typedef int16_t gvar_t;

#if MAX_ROTARY_ENCODERS > 0
  #define FLIGHT_MODE_ROTARY_ENCODERS_FIELD int16_t rotaryEncoders[MAX_ROTARY_ENCODERS];
#else
  #define FLIGHT_MODE_ROTARY_ENCODERS_FIELD
#endif

#if defined(CPUARM)
PACK(struct FlightModeData {
  trim_t trim[NUM_STICKS+NUM_AUX_TRIMS];
  NOBACKUP(char name[LEN_FLIGHT_MODE_NAME]);
  int16_t swtch:9;       // swtch of phase[0] is not used
  int16_t spare:7;
  uint8_t fadeIn;
  uint8_t fadeOut;
  FLIGHT_MODE_ROTARY_ENCODERS_FIELD
  gvar_t gvars[MAX_GVARS];
});
#elif !defined(PCBSTD)
PACK(struct FlightModeData {
  trim_t trim[NUM_STICKS];
  int8_t swtch;       // swtch of phase[0] is not used
  NOBACKUP(char name[LEN_FLIGHT_MODE_NAME]);
  uint8_t fadeIn:4;
  uint8_t fadeOut:4;
  FLIGHT_MODE_ROTARY_ENCODERS_FIELD
  gvar_t gvars[MAX_GVARS];
});
#else
PACK(struct FlightModeData {
  int8_t trim[NUM_STICKS];
  int8_t trim_ext;    // 2 extra bits per trim (10bits)
  int8_t swtch;       // swtch of phase[0] is not used
  NOBACKUP(char name[LEN_FLIGHT_MODE_NAME]);
  uint8_t fadeIn:4;
  uint8_t fadeOut:4;
});
#endif

/*
 * Curve structure
 */

#if defined(XCURVES)
PACK(struct CurveData {
  uint8_t type:1;
  uint8_t smooth:1;
  int8_t  points:6; // TODO conversion
  NOBACKUP(char name[LEN_CURVE_NAME]);
});
#elif defined(CPUARM)
typedef int16_t CurveData;
#else
typedef int8_t CurveData;
#endif

/*
 * GVar structure
 */

#if !defined(PCBSTD)
PACK(struct GVarData {
  NOBACKUP(char name[LEN_GVAR_NAME]);
  uint32_t min:12;
  uint32_t max:12;
  uint32_t popup:1;
  uint32_t prec:1;
  uint32_t unit:2;
  uint32_t spare:4;
});
#endif

/*
 * Timer structure
 */

#if defined(CPUARM)
PACK(struct TimerData {
  int32_t  mode:9;            // timer trigger source -> off, abs, stk, stk%, sw/!sw, !m_sw/!m_sw
  uint32_t start:23;
  int32_t  value:24;
  uint32_t countdownBeep:2;
  uint32_t minuteBeep:1;
  uint32_t persistent:2;
  int32_t  countdownStart:2;
  uint32_t direction:1;
  NOBACKUP(char name[LEN_TIMER_NAME]);
});
#elif defined(CPUM2560)
PACK(struct TimerData {
  int8_t   mode;            // timer trigger source -> off, abs, stk, stk%, sw/!sw, !m_sw/!m_sw
  uint16_t start;
  uint8_t  countdownBeep:2;
  uint8_t  minuteBeep:1;
  uint8_t  persistent:2;
  uint8_t  countdownStart:3;
  uint16_t value;
});
#else
PACK(struct TimerData {
  int8_t    mode;            // timer trigger source -> off, abs, stk, stk%, sw/!sw, !m_sw/!m_sw
  uint16_t  start:12;
  uint16_t  countdownBeep:1;
  uint16_t  minuteBeep:1;
  uint16_t  spare:2;
});
#endif

/*
 * Swash Ring structure
 */

#if defined(VIRTUALINPUTS)
PACK(struct SwashRingData {
  uint8_t   type;
  uint8_t   value;
  uint8_t   collectiveSource;
  uint8_t   aileronSource;
  uint8_t   elevatorSource;
  int8_t    collectiveWeight;
  int8_t    aileronWeight;
  int8_t    elevatorWeight;
});
#else
PACK(struct SwashRingData {
  uint8_t   invertELE:1;
  uint8_t   invertAIL:1;
  uint8_t   invertCOL:1;
  uint8_t   type:5;
  uint8_t   collectiveSource;
  uint8_t   value;
});
#endif

#if MAX_SCRIPTS > 0
PACK(struct ScriptData {
  char    file[LEN_SCRIPT_FILENAME];
  char    name[LEN_SCRIPT_NAME];
  int16_t  inputs[MAX_SCRIPT_INPUTS];
});
#endif

/*
 * Frsky Telemetry structure
 */

PACK(struct FrSkyRSSIAlarm {
  int8_t level:2;
  int8_t value:6;
});

#if defined(CPUARM)
typedef int16_t ls_telemetry_value_t;
#else
typedef uint8_t ls_telemetry_value_t;
#endif

#if !defined(COLORLCD)
PACK(struct FrSkyBarData {
  source_t source;
  ls_telemetry_value_t barMin;           // minimum for bar display
  ls_telemetry_value_t barMax;           // ditto for max display (would usually = ratio)
});

PACK(struct FrSkyLineData {
  source_t sources[NUM_LINE_ITEMS];
});

#if defined(PCBTARANIS)
PACK(struct TelemetryScriptData {
  char    file[LEN_SCRIPT_FILENAME];
  int16_t inputs[MAX_TELEM_SCRIPT_INPUTS];
});
#endif

union FrSkyScreenData {
  FrSkyBarData  bars[4];
  FrSkyLineData lines[4];
#if defined(PCBTARANIS)
  TelemetryScriptData script;
#endif
};
#endif

#if defined(COLORLCD)
PACK(struct FrSkyTelemetryData {
  uint8_t varioSource:7;
  uint8_t varioCenterSilent:1;
  int8_t  varioCenterMax;
  int8_t  varioCenterMin;
  int8_t  varioMin;
  int8_t  varioMax;
  FrSkyRSSIAlarm rssiAlarms[2];
});
#elif defined(CPUARM)
PACK(struct FrSkyChannelData {
  uint8_t   ratio;              // 0.0 means not used, 0.1V steps EG. 6.6 Volts = 66. 25.1V = 251, etc.
  int16_t   offset:12;
  uint16_t  type:4;             // channel unit (0=volts, ...)
  uint8_t   alarms_value[2];    // 0.1V steps EG. 6.6 Volts = 66. 25.1V = 251, etc.
  uint8_t   alarms_level:4;
  uint8_t   alarms_greater:2;   // 0=LT(<), 1=GT(>)
  uint8_t   spare:2;
  uint8_t   multiplier;         // 0=no multiplier, 1=*2 multiplier
});

PACK(struct FrSkyTelemetryData {
  uint8_t voltsSource;
  uint8_t altitudeSource;
  uint8_t screensType; // 2bits per screen (None/Gauges/Numbers/Script)
  FrSkyScreenData screens[MAX_TELEMETRY_SCREENS];
  uint8_t varioSource:7;
  uint8_t varioCenterSilent:1;
  int8_t  varioCenterMax;
  int8_t  varioCenterMin;
  int8_t  varioMin;
  int8_t  varioMax;
  FrSkyRSSIAlarm rssiAlarms[2];
});
#else
PACK(struct FrSkyChannelData {
  uint8_t   ratio;              // 0.0 means not used, 0.1V steps EG. 6.6 Volts = 66. 25.1V = 251, etc.
  int16_t   offset:12;
  uint16_t  type:4;             // channel unit (0=volts, ...)
  uint8_t   alarms_value[2];    // 0.1V steps EG. 6.6 Volts = 66. 25.1V = 251, etc.
  uint8_t   alarms_level:4;
  uint8_t   alarms_greater:2;   // 0=LT(<), 1=GT(>)
  uint8_t   multiplier:2;       // 0=no multiplier, 1=*2 multiplier
});

PACK(struct FrSkyTelemetryData {
  FrSkyChannelData channels[MAX_FRSKY_A_CHANNELS];
  uint8_t usrProto:2; // Protocol in FrSky user data, 0=None, 1=FrSky hub, 2=WS HowHigh, 3=Halcyon
  uint8_t blades:2;   // How many blades for RPMs, 0=2 blades
  uint8_t screensType:2;
  uint8_t voltsSource:2;
  int8_t  varioMin:4;
  int8_t  varioMax:4;
  FrSkyRSSIAlarm rssiAlarms[2];
  FrSkyScreenData screens[MAX_TELEMETRY_SCREENS];
  uint8_t varioSource:3;
  int8_t  varioCenterMin:5;
  uint8_t currentSource:3;
  int8_t  varioCenterMax:5;
  int8_t  fasOffset;
});
#endif

/*
 * MAVLINK Telemetry structure
 */

#if defined(MAVLINK)
PACK(struct MavlinkTelemetryData {
  uint8_t rc_rssi_scale:4;
  uint8_t pc_rssi_en:1;
  uint8_t spare1:3;
  uint8_t spare2[3];
});
#endif

/*
 * Telemetry Sensor structure
 */

#if defined(CPUARM)
PACK(struct TelemetrySensor {
  union {
    uint16_t id;                   // data identifier, for FrSky we can reuse existing ones. Source unit is derived from type.
    uint16_t persistentValue;
  };
  union {
    uint8_t instance;              // instance ID to allow handling multiple instances of same value type, for FrSky can be the physical ID of the sensor
    uint8_t formula;
  };
  char     label[TELEM_LABEL_LEN]; // user defined label
  uint8_t  type:1;                 // 0=custom / 1=calculated
  uint8_t  unit:5;                 // user can choose what unit to display each value in
  uint8_t  prec:2;
  uint8_t  autoOffset:1;
  uint8_t  filter:1;
  uint8_t  logs:1;
  uint8_t  persistent:1;
  uint8_t  onlyPositive:1;
  uint8_t  subId:3;
  union {
    PACK(struct {
      uint16_t ratio;
      int16_t  offset;
    }) custom;
    PACK(struct {
      uint8_t source;
      uint8_t index;
      uint16_t spare;
    }) cell;
    PACK(struct {
      int8_t sources[4];
    }) calc;
    PACK(struct {
      uint8_t source;
      uint8_t spare[3];
    }) consumption;
    PACK(struct {
      uint8_t gps;
      uint8_t alt;
      uint16_t spare;
    }) dist;
    uint32_t param;
  };
  NOBACKUP(
  void init(const char *label, uint8_t unit=UNIT_RAW, uint8_t prec=0);
  void init(uint16_t id);
  bool isAvailable() const;
  int32_t getValue(int32_t value, uint8_t unit, uint8_t prec) const;
  bool isConfigurable() const;
  bool isPrecConfigurable() const;
  int32_t getPrecMultiplier() const;
  int32_t getPrecDivisor() const);
});
#endif

/*
 * Module structure
 */

PACK(struct ModuleData {
  uint8_t type:4;
  int8_t  rfProtocol:4;
  uint8_t channelsStart;
  int8_t  channelsCount; // 0=8 channels
  uint8_t failsafeMode:4;  //only 3 bits used
  uint8_t subType:3;
  uint8_t invertedSerial:1; // telemetry serial inverted from standard
  int16_t failsafeChannels[NUM_CHNOUT];
  union {
    struct {
      int8_t  delay:6;
      uint8_t pulsePol:1;      // For PXX: false = internal antenna, true = exetrnal antenna
      uint8_t outputType:1;    // false = open drain, true = push pull
      int8_t  frameLength;
    } ppm;
    NOBACKUP(struct {
      uint8_t rfProtocol:6;    // can be changed to rfProtocol if rfProtocol gets more bits, currently 6 bits used
      uint8_t autoBindMode:1;
      uint8_t lowPowerMode:1;
      int8_t optionValue;
    } multi);
  };
});

/*
 * Model structure
 */

#if defined(CPUARM) || defined(CPUM2560)
typedef uint16_t BeepANACenter;
#else
typedef uint8_t BeepANACenter;
#endif

#if LEN_BITMAP_NAME > 0
#define MODEL_HEADER_BITMAP_FIELD      NOBACKUP(char bitmap[LEN_BITMAP_NAME]);
#else
#define MODEL_HEADER_BITMAP_FIELD
#endif


PACK(struct ModelHeader {
  char      name[LEN_MODEL_NAME]; // must be first for eeLoadModelName
  uint8_t   modelId[NUM_MODULES];
  MODEL_HEADER_BITMAP_FIELD
});

#if defined(COLORLCD)
typedef uint16_t swconfig_t;
typedef uint32_t swarnstate_t;
#elif defined(PCBTARANIS) && defined(REV9E)
typedef uint64_t swconfig_t;
typedef uint64_t swarnstate_t;
typedef uint32_t swarnenable_t;
#elif defined(PCBTARANIS)
typedef uint16_t swconfig_t;
typedef uint16_t swarnstate_t;
typedef uint8_t swarnenable_t;
#else
typedef uint8_t swarnstate_t;
typedef uint8_t swarnenable_t;
#endif

#if defined(COLORLCD)
  #define SWITCHES_WARNING_DATA \
    NOBACKUP(swarnstate_t  switchWarningState);
#else
  #define SWITCHES_WARNING_DATA \
    swarnstate_t  switchWarningState; \
    swarnenable_t switchWarningEnable;
#endif

#if defined(PCBSTD) && defined(GVARS)
  #define MODEL_GVARS_DATA gvar_t gvars[MAX_GVARS];
#elif defined(PCBSTD)
  #define MODEL_GVARS_DATA
#else
  #define MODEL_GVARS_DATA GVarData gvars[MAX_GVARS];
#endif

#if defined(MAVLINK)
  #define TELEMETRY_DATA MavlinkTelemetryData mavlink;
#elif defined(FRSKY) || !defined(PCBSTD)
  #define TELEMETRY_DATA NOBACKUP(FrSkyTelemetryData frsky);
#else
  #define TELEMETRY_DATA
#endif

#if defined(PCBHORUS)
#include "gui/horus/layout.h"
#include "gui/horus/topbar.h"
PACK(struct CustomScreenData {
  char layoutName[10];
  Layout::PersistentData layoutData;
});
#define CUSTOM_SCREENS_DATA \
  NOBACKUP(CustomScreenData screenData[MAX_CUSTOM_SCREENS]); \
  NOBACKUP(Topbar::PersistentData topbarData);
#else
#define CUSTOM_SCREENS_DATA
// TODO other boards could have their custom screens here as well
#endif

#if defined(PCBHORUS)
  #define MODELDATA_EXTRA   NOBACKUP(uint8_t spare:3); NOBACKUP(uint8_t trainerMode:3); NOBACKUP(uint8_t potsWarnMode:2); ModuleData moduleData[NUM_MODULES+1]; NOBACKUP(ScriptData scriptsData[MAX_SCRIPTS]); NOBACKUP(char inputNames[MAX_INPUTS][LEN_INPUT_NAME]); NOBACKUP(uint8_t potsWarnEnabled); NOBACKUP(int8_t potsWarnPosition[NUM_POTS]);
#elif defined(PCBFLAMENCO)
  #define MODELDATA_EXTRA   uint8_t spare:3; uint8_t trainerMode:3; uint8_t potsWarnMode:2; ModuleData moduleData[NUM_MODULES+1]; ScriptData scriptsData[MAX_SCRIPTS]; char inputNames[MAX_INPUTS][LEN_INPUT_NAME]; uint8_t potsWarnEnabled; int8_t potsWarnPosition[NUM_POTS];
#elif defined(PCBTARANIS)
  #define MODELDATA_EXTRA   uint8_t spare:3; uint8_t trainerMode:3; uint8_t potsWarnMode:2; ModuleData moduleData[NUM_MODULES+1]; ScriptData scriptsData[MAX_SCRIPTS]; char inputNames[MAX_INPUTS][LEN_INPUT_NAME]; uint8_t potsWarnEnabled; int8_t potsWarnPosition[NUM_POTS];
#elif defined(PCBSKY9X)
  #define MODELDATA_EXTRA   uint8_t spare:3; uint8_t trainerMode:3; uint8_t potsWarnMode:2; ModuleData moduleData[NUM_MODULES+1]; char inputNames[MAX_INPUTS][LEN_INPUT_NAME]; uint8_t potsWarnEnabled; int8_t potsWarnPosition[NUM_POTS];
//  #define MODELDATA_EXTRA   uint8_t spare:6; uint8_t potsWarnMode:2; ModuleData moduleData[NUM_MODULES+1]; uint8_t potsWarnEnabled; int8_t potsWarnPosition[NUM_POTS]; uint8_t rxBattAlarms[2]; char inputNames[MAX_INPUTS][LEN_INPUT_NAME];
#else
  #define MODELDATA_EXTRA
#endif

PACK(struct ModelData {
  ModelHeader header;
  TimerData timers[MAX_TIMERS];
  AVR_FIELD(uint8_t   protocol:3)
  ARM_FIELD(uint8_t   telemetryProtocol:3)
  uint8_t   thrTrim:1;            // Enable Throttle Trim
  AVR_FIELD(int8_t    ppmNCH:4)
  ARM_FIELD(uint8_t   noGlobalFunctions:1)
  ARM_FIELD(uint8_t   displayTrims:2)
  ARM_FIELD(uint8_t   ignoreSensorIds:1)
  int8_t    trimInc:3;            // Trim Increments
  uint8_t   disableThrottleWarning:1;
  ARM_FIELD(uint8_t displayChecklist:1)
  AVR_FIELD(uint8_t pulsePol:1)
  uint8_t   extendedLimits:1;
  uint8_t   extendedTrims:1;
  uint8_t   throttleReversed:1;
  AVR_FIELD(int8_t ppmDelay)
  BeepANACenter beepANACenter;
  MixData   mixData[MAX_MIXERS];
  LimitData limitData[NUM_CHNOUT];
  ExpoData  expoData[MAX_EXPOS];

  CurveData curves[MAX_CURVES];
  int8_t    points[NUM_POINTS];

  LogicalSwitchData logicalSw[NUM_LOGICAL_SWITCH];
  CustomFunctionData customFn[NUM_CFN];
  SwashRingData swashR;
  FlightModeData flightModeData[MAX_FLIGHT_MODES];

  AVR_FIELD(int8_t ppmFrameLength)     // 0=22.5ms  (10ms-30ms) 0.5ms increments

  NOBACKUP(uint8_t thrTraceSrc);

  SWITCHES_WARNING_DATA

  MODEL_GVARS_DATA

  TELEMETRY_DATA

  MODELDATA_EXTRA

  ARM_FIELD(NOBACKUP(TelemetrySensor telemetrySensors[MAX_SENSORS]))

  TARANIS_REV9E_FIELD(uint8_t toplcdTimer)

  CUSTOM_SCREENS_DATA

});

/*
 * Radio structure
 */

#if XPOTS_MULTIPOS_COUNT > 0
PACK(struct StepsCalibData {
  uint8_t count;
  uint8_t steps[XPOTS_MULTIPOS_COUNT-1];
});
#endif

PACK(struct CalibData {
  int16_t mid;
  int16_t spanNeg;
  int16_t spanPos;
});

PACK(struct TrainerMix {
  uint8_t srcChn:6; // 0-7 = ch1-8
  uint8_t mode:2;   // off,add-mode,subst-mode
  int8_t  studWeight;
});

PACK(struct TrainerData {
  int16_t        calib[4];
  NOBACKUP(TrainerMix mix[4]);
});

#if defined(PCBHORUS)
  #define SPLASH_MODE
#elif defined(FSPLASH)
  #define SPLASH_MODE uint8_t splashMode:3
#elif defined(PCBTARANIS)
  #define SPLASH_MODE int8_t splashMode:3
#else
  #define SPLASH_MODE uint8_t splashMode:1; uint8_t splashSpare:2
#endif

#if defined(CPUARM)
  #define EXTRA_GENERAL_FIELDS_ARM \
    NOBACKUP(uint8_t  backlightBright); \
    NOBACKUP(uint32_t globalTimer); \
    NOBACKUP(uint8_t  btBaudrate); \
    NOBACKUP(uint8_t  countryCode); \
    NOBACKUP(uint8_t  imperial:1); \
    NOBACKUP(uint8_t  jitterFilter:1); /* 0 - active */\
    NOBACKUP(uint8_t  spareExtra:6); \
    NOBACKUP(char     ttsLanguage[2]); \
    NOBACKUP(int8_t   beepVolume:4); \
    NOBACKUP(int8_t   wavVolume:4); \
    NOBACKUP(int8_t   varioVolume:4); \
    NOBACKUP(int8_t   backgroundVolume:4); \
    NOBACKUP(int8_t   varioPitch); \
    NOBACKUP(int8_t   varioRange); \
    NOBACKUP(int8_t   varioRepeat); \
    CustomFunctionData customFn[NUM_CFN];
#endif

#if defined(PCBHORUS)
  #define EXTRA_GENERAL_FIELDS \
    EXTRA_GENERAL_FIELDS_ARM \
    NOBACKUP(uint8_t  serial2Mode:6); \
    uint8_t  slidersConfig:2; \
    uint32_t switchConfig; \
    uint8_t  potsConfig; /* two bits per pot */ \
    NOBACKUP(char switchNames[NUM_SWITCHES][LEN_SWITCH_NAME]); \
    NOBACKUP(char anaNames[NUM_STICKS+NUM_POTS][LEN_ANA_NAME]); \
    NOBACKUP(char currModelFilename[LEN_MODEL_FILENAME+1]); \
    NOBACKUP(uint8_t bluetoothEnable:1); \
    NOBACKUP(uint8_t blOffBright:7); \
    NOBACKUP(char bluetoothName[LEN_BLUETOOTH_NAME]);
#elif defined(PCBFLAMENCO)
  #define EXTRA_GENERAL_FIELDS \
    EXTRA_GENERAL_FIELDS_ARM \
    uint8_t  serial2Mode:6; \
    uint8_t  spare:2; \
    uint32_t switchConfig; \
    uint8_t  potsType; /*two bits for every pot*/\
    char switchNames[NUM_SWITCHES][LEN_SWITCH_NAME]; \
    char anaNames[NUM_STICKS+NUM_POTS][LEN_ANA_NAME];
#elif defined(PCBTARANIS)
  #if defined(REV9E)
    #define BLUETOOTH_FIELDS \
      uint8_t bluetoothEnable; \
      char bluetoothName[LEN_BLUETOOTH_NAME];
  #else
    #define BLUETOOTH_FIELDS
  #endif
  #define EXTRA_GENERAL_FIELDS \
    EXTRA_GENERAL_FIELDS_ARM \
    uint8_t  serial2Mode:6; \
    uint8_t  slidersConfig:2; \
    uint8_t  potsConfig; /* two bits per pot */\
    uint8_t  backlightColor; \
    swarnstate_t switchUnlockStates; \
    swconfig_t switchConfig; \
    char switchNames[NUM_SWITCHES][LEN_SWITCH_NAME]; \
    char anaNames[NUM_STICKS+NUM_POTS][LEN_ANA_NAME]; \
    BLUETOOTH_FIELDS
#elif defined(PCBSKY9X)
  #define EXTRA_GENERAL_FIELDS \
    EXTRA_GENERAL_FIELDS_ARM \
    int8_t   txCurrentCalibration; \
    int8_t   temperatureWarn; \
    uint8_t  mAhWarn; \
    uint16_t mAhUsed; \
    int8_t   temperatureCalib; \
    uint8_t  optrexDisplay; \
    uint8_t  sticksGain; \
    uint8_t  rotarySteps; \
    uint32_t switchConfig; \
    char switchNames[NUM_SWITCHES][LEN_SWITCH_NAME]; \
    char anaNames[NUM_STICKS+NUM_POTS][LEN_ANA_NAME];
#elif defined(CPUARM)
  #define EXTRA_GENERAL_FIELDS EXTRA_GENERAL_FIELDS_ARM
#elif defined(PXX)
  #define EXTRA_GENERAL_FIELDS uint8_t countryCode;
#else
  #define EXTRA_GENERAL_FIELDS
#endif

#if defined(COLORLCD)
  #include "gui/horus/theme.h"
  #define THEME_DATA \
    NOBACKUP(char themeName[8]); \
    NOBACKUP(Theme::PersistentData themeData);
#else
  #define THEME_DATA
#endif

PACK(struct RadioData {
  NOBACKUP(uint8_t version);
  NOBACKUP(uint16_t variant);
  CalibData calib[NUM_STICKS+NUM_POTS];
  NOBACKUP(uint16_t chkSum);
  N_HORUS_FIELD(int8_t currModel);
  N_HORUS_FIELD(uint8_t contrast);
  NOBACKUP(uint8_t vBatWarn);
  NOBACKUP(int8_t txVoltageCalibration);
  NOBACKUP(int8_t backlightMode);
  NOBACKUP(TrainerData trainer);
  NOBACKUP(uint8_t view);            // index of view in main screen
  NOBACKUP(int8_t buzzerMode:2);    // -2=quiet, -1=only alarms, 0=no keys, 1=all
  NOBACKUP(uint8_t fai:1);
  NOBACKUP(int8_t beepMode:2);      // -2=quiet, -1=only alarms, 0=no keys, 1=all
  NOBACKUP(uint8_t alarmsFlash:1);
  NOBACKUP(uint8_t disableMemoryWarning:1);
  NOBACKUP(uint8_t disableAlarmWarning:1);
  uint8_t stickMode:2;
  NOBACKUP(int8_t timezone:5);
  NOBACKUP(uint8_t adjustRTC:1);
  NOBACKUP(uint8_t inactivityTimer);
  NOBACKUP(uint8_t mavbaud:3);
  SPLASH_MODE; /* 3bits */
  NOBACKUP(int8_t hapticMode:2);    // -2=quiet, -1=only alarms, 0=no keys, 1=all
  AVR_FIELD(uint8_t blOffBright:4)
  AVR_FIELD(uint8_t blOnBright:4)
  ARM_FIELD(int8_t switchesDelay)
  NOBACKUP(uint8_t lightAutoOff);
  NOBACKUP(uint8_t templateSetup);   // RETA order for receiver channels
  NOBACKUP(int8_t PPM_Multiplier);
  NOBACKUP(int8_t hapticLength);
  N_HORUS_FIELD(N_TARANIS_FIELD(N_PCBSTD_FIELD(uint8_t reNavigation)));
  N_HORUS_FIELD(N_TARANIS_FIELD(uint8_t stickReverse));
  NOBACKUP(int8_t beepLength:3);
  NOBACKUP(int8_t hapticStrength:3);
  NOBACKUP(uint8_t gpsFormat:1);
  NOBACKUP(uint8_t unexpectedShutdown:1);
  NOBACKUP(uint8_t speakerPitch);
  NOBACKUP(int8_t speakerVolume);
  NOBACKUP(int8_t vBatMin);
  NOBACKUP(int8_t vBatMax);

  EXTRA_GENERAL_FIELDS

  THEME_DATA

});

#undef SWITCHES_WARNING_DATA
#undef MODEL_GVARS_DATA
#undef TELEMETRY_DATA
#undef MODELDATA_EXTRA
#undef CUSTOM_SCREENS_DATA
#undef SPLASH_MODE
#undef EXTRA_GENERAL_FIELDS
#undef THEME_DATA
#undef NOBACKUP


#if !defined(BACKUP)
/* Compile time check to test structure size has not changed *
   Changing the size of one of the eeprom structs may cause wrong data to
   be loaded. Error out if the struct size changes.
   This function tries not avoid checking or using the defines
   other than the CPU arch and board type so changes in other
   defines also trigger the struct size changes */

template <typename ToCheck, size_t expectedSize, size_t realSize = sizeof(ToCheck)>
void check_size() {
  static_assert(expectedSize == realSize, "struct size changed");
}

static inline void check_struct()
{
#define CHKSIZE(x, y) check_size<struct x, y>()
#define CHKTYPE(x, y) check_size<x, y>()

#if defined(VIRTUALINPUTS)
  CHKSIZE(CurveRef, 2);
#endif

  /* Difference between Taranis/Horus is LEN_EXPOMIX_NAME */
  /* Sky9x does not have virtualinputs */

  /* LEN_CFN_NAME is the difference in CustomFunctionData */

#if defined(PCBTARANIS)
  CHKSIZE(MixData, 22);
  CHKSIZE(ExpoData, 19);
  CHKSIZE(LimitData, 13);
  CHKSIZE(LogicalSwitchData, 9);
  CHKSIZE(CustomFunctionData, 11);
  CHKSIZE(FlightModeData, 40);
  CHKSIZE(TimerData, 16);
  CHKSIZE(SwashRingData, 8);
  CHKSIZE(FrSkyBarData, 6);
  CHKSIZE(FrSkyLineData, 6);
  CHKTYPE(union FrSkyScreenData, 24);
  CHKSIZE(FrSkyTelemetryData, 106);
  CHKSIZE(ModelHeader, 24);
  CHKSIZE(CurveData, 4);

#if defined(REV9E)
  CHKSIZE(RadioData, 952);
  CHKSIZE(ModelData, 6519);
#else
  CHKSIZE(RadioData, 872);
  CHKSIZE(ModelData, 6506);
#endif

#elif defined(PCBHORUS)
  CHKSIZE(MixData, 20);
  CHKSIZE(ExpoData, 17);
  CHKSIZE(LimitData, 13);
  CHKSIZE(CustomFunctionData, 9);
  CHKSIZE(FlightModeData, 44);
  CHKSIZE(TimerData, 16);
  CHKSIZE(SwashRingData, 8);

  CHKSIZE(FrSkyTelemetryData, 7);
  CHKSIZE(ModelHeader, 27);
  CHKSIZE(CurveData, 4);
  CHKSIZE(RadioData, 835);
  CHKSIZE(ModelData, 9371);

#elif defined(PCBSKY9X)
  CHKSIZE(MixData, 20);
  CHKSIZE(ExpoData, 17);
  CHKSIZE(LimitData, 13);
  CHKSIZE(CustomFunctionData, 9);
  CHKSIZE(FlightModeData, 38);
  CHKSIZE(TimerData, 11);
  CHKSIZE(SwashRingData, 8);
  CHKSIZE(FrSkyBarData, 5);
  CHKSIZE(FrSkyLineData, 2);
  CHKSIZE(FrSkyTelemetryData, 90);
  CHKSIZE(ModelHeader, 12);
  CHKSIZE(CurveData, 4);
  CHKSIZE(RadioData, 731);
  CHKSIZE(ModelData, 5282);
#else
  // Common for all variants
  CHKSIZE(LimitData, 5);
  CHKSIZE(SwashRingData, 3);
  CHKSIZE(FrSkyBarData, 3);
  CHKSIZE(FrSkyLineData, 2);
  CHKSIZE(FrSkyTelemetryData, 43);
  CHKSIZE(ModelHeader, 11);
  CHKTYPE(CurveData, 1);

  // AVR
#if defined(CPUM2560) || defined(CPUM2561)
  CHKSIZE(ExpoData, 5);
  CHKSIZE(MixData, 10);
#else
  CHKSIZE(MixData, 9);
  CHKSIZE(ExpoData, 4);
#endif

#if defined(CPUM2560)
  CHKSIZE(CustomFunctionData, 4);
  CHKSIZE(TimerData, 6);
#else
  CHKSIZE(CustomFunctionData, 3);
  CHKSIZE(TimerData, 3);
#endif

#if defined(PCBSTD)
  CHKSIZE(FlightModeData, 13);
  CHKSIZE(RadioData, 84);
#else
  CHKSIZE(FlightModeData, 30);
  CHKSIZE(RadioData, 85);
#endif

#endif /* board specific ifdefs*/

#if defined(CPUARM)
  CHKSIZE(LogicalSwitchData, 9);
#if !defined(COLORLCD)
  CHKSIZE(FrSkyChannelData, 7);
#endif
  CHKSIZE(TelemetrySensor, 13);
  CHKSIZE(ModuleData,70);
#else
  CHKSIZE(LogicalSwitchData, 3);
  CHKSIZE(FrSkyChannelData, 6);
  CHKSIZE(ModuleData, 38);
#endif

#if !defined(PCBSTD)
  CHKSIZE(GVarData, 7);
#endif

  CHKSIZE(FrSkyRSSIAlarm, 1);
  CHKSIZE(TrainerData, 16);

#undef CHKSIZE
#undef CHKSIZEUNION
}
#endif /* BACKUP */
