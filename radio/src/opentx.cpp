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

#include "opentx.h"

RadioData  g_eeGeneral;
ModelData  g_model;

#if defined(SDCARD)
Clipboard clipboard;
#endif

#if defined(PCBTARANIS)
uint8_t modelBitmap[MODEL_BITMAP_SIZE] __DMA;

bool loadModelBitmap(char * name, uint8_t * bitmap)
{
  uint8_t len = zlen(name, LEN_BITMAP_NAME);
  if (len > 0) {
    char lfn[] = BITMAPS_PATH "/xxxxxxxxxx.bmp";
    strncpy(lfn+sizeof(BITMAPS_PATH), name, len);
    strcpy(lfn+sizeof(BITMAPS_PATH)+len, BITMAPS_EXT);
    if (lcdLoadBitmap(bitmap, lfn, MODEL_BITMAP_WIDTH, MODEL_BITMAP_HEIGHT)) {
      return true;
    }
  }

  // In all error cases, we set the default logo
  memcpy(bitmap, logo_taranis, MODEL_BITMAP_SIZE);
  return false;
}
#endif

#if !defined(CPUARM)
uint8_t g_tmr1Latency_max;
uint8_t g_tmr1Latency_min;
uint16_t lastMixerDuration;
#endif

uint8_t unexpectedShutdown = 0;

/* AVR: mixer duration in 1/16ms */
/* ARM: mixer duration in 0.5us */
uint16_t maxMixerDuration;

#if defined(AUDIO) && !defined(CPUARM)
audioQueue  audio;
#endif

uint8_t heartbeat;

#if defined(OVERRIDE_CHANNEL_FUNCTION)
safetych_t safetyCh[NUM_CHNOUT];
#endif

union ReusableBuffer reusableBuffer;

const pm_uint8_t bchout_ar[] PROGMEM = {
    0x1B, 0x1E, 0x27, 0x2D, 0x36, 0x39,
    0x4B, 0x4E, 0x63, 0x6C, 0x72, 0x78,
    0x87, 0x8D, 0x93, 0x9C, 0xB1, 0xB4,
    0xC6, 0xC9, 0xD2, 0xD8, 0xE1, 0xE4 };

uint8_t channel_order(uint8_t x)
{
  return ( ((pgm_read_byte(bchout_ar + g_eeGeneral.templateSetup) >> (6-(x-1) * 2)) & 3 ) + 1 );
}

/*
mode1 rud ele thr ail
mode2 rud thr ele ail
mode3 ail ele thr rud
mode4 ail thr ele rud
*/
const pm_uint8_t modn12x3[] PROGMEM = {
    0, 1, 2, 3,
    0, 2, 1, 3,
    3, 1, 2, 0,
    3, 2, 1, 0 };

volatile tmr10ms_t g_tmr10ms;

#if defined(CPUARM)
volatile uint8_t rtc_count = 0;
uint32_t watchdogTimeout = 0;

void watchdogSetTimeout(uint32_t timeout)
{
  watchdogTimeout = timeout;
}
#endif

void per10ms()
{
  g_tmr10ms++;

#if defined(CPUARM)
  if (watchdogTimeout) {
    watchdogTimeout -= 1;
    wdt_reset();  // Retrigger hardware watchdog
  }
#endif

#if defined(GUI)
  if (lightOffCounter) lightOffCounter--;
  if (flashCounter) flashCounter--;
  if (noHighlightCounter) noHighlightCounter--;
#endif

  if (trimsCheckTimer) trimsCheckTimer--;
  if (ppmInputValidityTimer) ppmInputValidityTimer--;

#if defined(CPUARM)
  if (trimsDisplayTimer)
    trimsDisplayTimer--;
  else
    trimsDisplayMask = 0;
#endif

#if defined(RTCLOCK)
  /* Update global Date/Time every 100 per10ms cycles */
  if (++g_ms100 == 100) {
    g_rtcTime++;   // inc global unix timestamp one second
#if defined(COPROCESSOR)
    if (g_rtcTime < 60 || rtc_count<5) {
      rtcInit();
      rtc_count++;
    }
    else {
      coprocReadData(true);
    }
#endif
    g_ms100 = 0;
  }
#endif

  readKeysAndTrims();

#if defined(ROTARY_ENCODER_NAVIGATION)
  if (IS_RE_NAVIGATION_ENABLE()) {
    static rotenc_t rePreviousValue;
    rotenc_t reNewValue = (g_rotenc[NAVIGATION_RE_IDX()] / ROTARY_ENCODER_GRANULARITY);
    int8_t scrollRE = reNewValue - rePreviousValue;
    if (scrollRE) {
      rePreviousValue = reNewValue;
      putEvent(scrollRE < 0 ? EVT_ROTARY_LEFT : EVT_ROTARY_RIGHT);
    }
    uint8_t evt = s_evt;
    if (EVT_KEY_MASK(evt) == BTN_REa + NAVIGATION_RE_IDX()) {
      if (IS_KEY_BREAK(evt)) {
        putEvent(EVT_ROTARY_BREAK);
      }
      else if (IS_KEY_LONG(evt)) {
        putEvent(EVT_ROTARY_LONG);
      }
    }
  }
#endif

#if defined(FRSKY) || defined(JETI)
  if (!IS_DSM2_SERIAL_PROTOCOL(s_current_protocol[0]))
    telemetryInterrupt10ms();
#endif

  // These moved here from evalFlightModeMixes() to improve beep trigger reliability.
#if defined(PWM_BACKLIGHT)
  if ((g_tmr10ms&0x03) == 0x00)
    backlightFade(); // increment or decrement brightness until target brightness is reached
#endif

#if !defined(AUDIO)
  if (mixWarning & 1) if(((g_tmr10ms&0xFF)==  0)) AUDIO_MIX_WARNING(1);
  if (mixWarning & 2) if(((g_tmr10ms&0xFF)== 64) || ((g_tmr10ms&0xFF)== 72)) AUDIO_MIX_WARNING(2);
  if (mixWarning & 4) if(((g_tmr10ms&0xFF)==128) || ((g_tmr10ms&0xFF)==136) || ((g_tmr10ms&0xFF)==144)) AUDIO_MIX_WARNING(3);
#endif

#if defined(SDCARD)
  sdPoll10ms();
#endif

  heartbeat |= HEART_TIMER_10MS;
}

FlightModeData *flightModeAddress(uint8_t idx)
{
  return &g_model.flightModeData[idx];
}

ExpoData *expoAddress(uint8_t idx )
{
  return &g_model.expoData[idx];
}

MixData *mixAddress(uint8_t idx)
{
  return &g_model.mixData[idx];
}

LimitData *limitAddress(uint8_t idx)
{
  return &g_model.limitData[idx];
}

#if defined(CPUM64)
void memclear(void *ptr, uint8_t size)
{
  memset(ptr, 0, size);
}
#endif

void memswap(void * a, void * b, uint8_t size)
{
  uint8_t * x = (uint8_t *)a;
  uint8_t * y = (uint8_t *)b;
  uint8_t temp ;

  while (size--) {
    temp = *x;
    *x++ = *y;
    *y++ = temp;
  }
}

void generalDefault()
{
  memclear(&g_eeGeneral, sizeof(g_eeGeneral));
  g_eeGeneral.version  = EEPROM_VER;
  g_eeGeneral.variant = EEPROM_VARIANT;

#if !defined(PCBHORUS)
  g_eeGeneral.contrast = 25;
#endif

#if defined(PCBFLAMENCO)
  g_eeGeneral.vBatWarn = 33;
  g_eeGeneral.vBatMin = -60; // 0 is 9.0V
  g_eeGeneral.vBatMax = -78; // 0 is 12.0V
#elif defined(PCBHORUS)
  g_eeGeneral.potsConfig = 0x05;    // S1 and S2 = pots with detent
  g_eeGeneral.slidersConfig = 0x03; // LS and RS = sliders with detent
  g_eeGeneral.blOffBright = 20;
#elif defined(PCBTARANIS)
  g_eeGeneral.potsConfig = 0x05;    // S1 and S2 = pots with detent
  g_eeGeneral.slidersConfig = 0x03; // LS and RS = sliders with detent
#endif

#if defined(PCBTARANIS) || defined(PCBHORUS)
  g_eeGeneral.switchConfig = 0x00007bff; // 6x3POS, 1x2POS, 1xTOGGLE
#endif

#if defined(PCBTARANIS) && defined(REV9E)
  // NI-MH 9.6V
  g_eeGeneral.vBatWarn = 87;
  g_eeGeneral.vBatMin = -5;
  g_eeGeneral.vBatMax = -5;
#elif defined(PCBTARANIS)
  // NI-MH 7.2V
  g_eeGeneral.vBatWarn = 65;
  g_eeGeneral.vBatMin = -30;
  g_eeGeneral.vBatMax = -40;
#else
  g_eeGeneral.vBatWarn = 90;
#endif

#if defined(DEFAULT_MODE)
  g_eeGeneral.stickMode = DEFAULT_MODE-1;
#endif

#if defined(PCBFLAMENCO)
  g_eeGeneral.templateSetup = 21; /* AETR */
#elif defined(PCBTARANIS)
  g_eeGeneral.templateSetup = 17; /* TAER */
#endif

#if defined(PCBFLAMENCO)
  g_eeGeneral.inactivityTimer = 50;
#elif !defined(CPUM64)
  g_eeGeneral.backlightMode = e_backlight_mode_all;
  g_eeGeneral.lightAutoOff = 2;
  g_eeGeneral.inactivityTimer = 10;
#endif

#if defined(CPUARM)
  g_eeGeneral.ttsLanguage[0] = 'e';
  g_eeGeneral.ttsLanguage[1] = 'n';
  g_eeGeneral.wavVolume = 2;
  g_eeGeneral.backgroundVolume = 1;
#endif

#if defined(CPUARM)
  for (int i=0; i<NUM_STICKS; ++i) {
    g_eeGeneral.trainer.mix[i].mode = 2;
    g_eeGeneral.trainer.mix[i].srcChn = channel_order(i+1) - 1;
    g_eeGeneral.trainer.mix[i].studWeight = 100;
  }
#endif

#if defined(PCBTARANIS) && defined(REV9E)
  const int8_t defaultName[] = { 20, -1, -18, -1, -14, -9, -19 };
  memcpy(g_eeGeneral.bluetoothName, defaultName, sizeof(defaultName));
#endif

#if !defined(EEPROM)
  strcpy(g_eeGeneral.currModelFilename, DEFAULT_MODEL_FILENAME);
#endif

#if defined(PCBHORUS)
  strcpy(g_eeGeneral.themeName, theme->getName());
  theme->init();
#endif

  g_eeGeneral.chkSum = 0xFFFF;
}

uint16_t evalChkSum()
{
  uint16_t sum = 0;
  const int16_t *calibValues = (const int16_t *) &g_eeGeneral.calib[0];
  for (int i=0; i<12; i++)
    sum += calibValues[i];
  return sum;
}

#if defined(VIRTUALINPUTS)
void clearInputs()
{
  memset(g_model.expoData, 0, sizeof(g_model.expoData)); // clear all expos
}

void defaultInputs()
{
  clearInputs();

  for (int i=0; i<NUM_STICKS; i++) {
    uint8_t stick_index = channel_order(i+1);
    ExpoData *expo = expoAddress(i);
    expo->srcRaw = MIXSRC_Rud - 1 + stick_index;
    expo->curve.type = CURVE_REF_EXPO;
    expo->chn = i;
    expo->weight = 100;
    expo->mode = 3; // TODO constant
#if defined(TRANSLATIONS_CZ)
    for (int c=0; c<4; c++) {
      g_model.inputNames[i][c] = char2idx(STR_INPUTNAMES[1+4*(stick_index-1)+c]);
    }
    g_model.inputNames[i][4] = '\0';
#else
    for (int c=0; c<3; c++) {
      g_model.inputNames[i][c] = char2idx(STR_VSRCRAW[2+4*stick_index+c]);
    }
    g_model.inputNames[i][3] = '\0';
#endif
  }
  storageDirty(EE_MODEL);
}
#endif

#if defined(TEMPLATES)
inline void applyDefaultTemplate()
{
  applyTemplate(TMPL_SIMPLE_4CH); // calls storageDirty internally
}
#else
void applyDefaultTemplate()
{
#if defined(VIRTUALINPUTS)
  defaultInputs(); // calls storageDirty internally
#else
  storageDirty(EE_MODEL);
#endif

  for (int i=0; i<NUM_STICKS; i++) {
    MixData * mix = mixAddress(i);
    mix->destCh = i;
    mix->weight = 100;
#if defined(VIRTUALINPUTS)
    mix->srcRaw = i+1;
#else
    mix->srcRaw = MIXSRC_Rud - 1 + channel_order(i+1);
#endif
  }
}
#endif

#if defined(CPUARM) && defined(EEPROM)
void checkModelIdUnique(uint8_t index, uint8_t module)
{
  uint8_t modelId = g_model.header.modelId[module];
  if (modelId != 0) {
    for (uint8_t i=0; i<MAX_MODELS; i++) {
      if (i != index) {
        for (uint8_t j=0; j<NUM_MODULES; j++) {
          if (modelId == modelHeaders[i].modelId[j]) {
            POPUP_WARNING(STR_MODELIDUSED);
            return;
          }
        }
      }
    }
  }
}
#endif

void modelDefault(uint8_t id)
{
  memset(&g_model, 0, sizeof(g_model));

  applyDefaultTemplate();

#if defined(LUA)
  if (isFileAvailable(WIZARD_PATH "/" WIZARD_NAME)) {
    f_chdir(WIZARD_PATH);
    luaExec(WIZARD_NAME);
  }
#endif

#if defined(PCBTARANIS)
  g_model.moduleData[INTERNAL_MODULE].type = MODULE_TYPE_XJT;
#elif defined(PCBSKY9X)
  g_model.moduleData[EXTERNAL_MODULE].type = MODULE_TYPE_PPM;
#endif

#if defined(CPUARM) && defined(EEPROM)
  for (int i=0; i<NUM_MODULES; i++) {
    modelHeaders[id].modelId[i] = g_model.header.modelId[i] = id+1;
  }
  checkModelIdUnique(id, 0);
#endif

#if defined(CPUARM) && defined(FLIGHT_MODES) && defined(GVARS)
  for (int p=1; p<MAX_FLIGHT_MODES; p++) {
    for (int i=0; i<MAX_GVARS; i++) {
      g_model.flightModeData[p].gvars[i] = GVAR_MAX+1;
    }
  }
#endif

#if defined(MAVLINK)
  g_model.mavlink.rc_rssi_scale = 15;
  g_model.mavlink.pc_rssi_en = 1;
#endif

#if !defined(EEPROM)
  strcpy(g_model.header.name, "\015\361\374\373\364");
  g_model.header.name[5] = '\033' + id/10;
  g_model.header.name[6] = '\033' + id%10;
#endif

#if defined(COLORLCD)
  extern const LayoutFactory * defaultLayout;
  delete customScreens[0];
  customScreens[0] = defaultLayout->create(&g_model.screenData[0].layoutData);
  strcpy(g_model.screenData[0].layoutName, "Layout2P1");
  extern const WidgetFactory * defaultWidget;
  customScreens[0]->createWidget(0, defaultWidget);
  // enable switch warnings
  for (int i=0; i<NUM_SWITCHES; i++) {
    g_model.switchWarningState |= (1 << (3*i));
  }
#endif
}

#if defined(VIRTUALINPUTS)
bool isInputRecursive(int index)
{
  ExpoData * line = expoAddress(0);
  for (int i=0; i<MAX_EXPOS; i++, line++) {
    if (line->chn > index)
      break;
    else if (line->chn < index)
      continue;
    else if (line->srcRaw >= MIXSRC_FIRST_LOGICAL_SWITCH)
      return true;
  }
  return false;
}
#endif

#if defined(AUTOSOURCE)
int8_t getMovedSource(GET_MOVED_SOURCE_PARAMS)
{
  int8_t result = 0;
  static tmr10ms_t s_move_last_time = 0;

#if defined(VIRTUALINPUTS)
  static int16_t inputsStates[MAX_INPUTS];
  if (min <= MIXSRC_FIRST_INPUT) {
    for (uint8_t i=0; i<MAX_INPUTS; i++) {
      if (abs(anas[i] - inputsStates[i]) > 512) {
        if (!isInputRecursive(i)) {
          result = MIXSRC_FIRST_INPUT+i;
          break;
        }
      }
    }
  }
#endif

  static int16_t sourcesStates[NUM_STICKS+NUM_POTS];
  if (result == 0) {
    for (uint8_t i=0; i<NUM_STICKS+NUM_POTS; i++) {
      if (abs(calibratedStick[i] - sourcesStates[i]) > 512) {
        result = MIXSRC_Rud+i;
        break;
      }
    }
  }

  bool recent = ((tmr10ms_t)(get_tmr10ms() - s_move_last_time) > 10);
  if (recent) {
    result = 0;
  }

  if (result || recent) {
#if defined(VIRTUALINPUTS)
    memcpy(inputsStates, anas, sizeof(inputsStates));
#endif
    memcpy(sourcesStates, calibratedStick, sizeof(sourcesStates));
  }

  s_move_last_time = get_tmr10ms();
  return result;
}
#endif

#if defined(FLIGHT_MODES)
uint8_t getFlightMode()
{
  for (uint8_t i=1; i<MAX_FLIGHT_MODES; i++) {
    FlightModeData *phase = &g_model.flightModeData[i];
    if (phase->swtch && getSwitch(phase->swtch)) {
      return i;
    }
  }
  return 0;
}
#endif

trim_t getRawTrimValue(uint8_t phase, uint8_t idx)
{
  FlightModeData *p = flightModeAddress(phase);
#if defined(PCBSTD)
  return (((trim_t)p->trim[idx]) << 2) + ((p->trim_ext >> (2*idx)) & 0x03);
#else
  return p->trim[idx];
#endif
}

int getTrimValue(uint8_t phase, uint8_t idx)
{
#if defined(VIRTUALINPUTS)
  int result = 0;
  for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
    trim_t v = getRawTrimValue(phase, idx);
    if (v.mode == TRIM_MODE_NONE) {
      return result;
    }
    else {
      unsigned int p = v.mode >> 1;
      if (p == phase || phase == 0) {
        return result + v.value;
      }
      else {
        phase = p;
        if (v.mode % 2 != 0) {
          result += v.value;
        }
      }
    }
  }
  return 0;
#else
  return getRawTrimValue(getTrimFlightPhase(phase, idx), idx);
#endif
}

#if defined(VIRTUALINPUTS)
bool setTrimValue(uint8_t phase, uint8_t idx, int trim)
{
  for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
    trim_t & v = flightModeAddress(phase)->trim[idx];
    if (v.mode == TRIM_MODE_NONE)
      return false;
    unsigned int p = v.mode >> 1;
    if (p == phase || phase == 0) {
      v.value = trim;
      break;
    }
    else if (v.mode % 2 == 0) {
      phase = p;
    }
    else {
      v.value = limit<int>(TRIM_EXTENDED_MIN, trim - getTrimValue(p, idx), TRIM_EXTENDED_MAX);
      break;
    }
  }
  storageDirty(EE_MODEL);
  return true;
}
#else
void setTrimValue(uint8_t phase, uint8_t idx, int trim)
{
#if defined(PCBSTD)
  FlightModeData *p = flightModeAddress(phase);
  p->trim[idx] = (int8_t)(trim >> 2);
  idx <<= 1;
  p->trim_ext = (p->trim_ext & ~(0x03 << idx)) + (((trim & 0x03) << idx));
#else
  FlightModeData *p = flightModeAddress(phase);
  p->trim[idx] = trim;
#endif
  storageDirty(EE_MODEL);
}
#endif

#if !defined(VIRTUALINPUTS)
uint8_t getTrimFlightPhase(uint8_t phase, uint8_t idx)
{
  for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
    if (phase == 0) return 0;
    trim_t trim = getRawTrimValue(phase, idx);
    if (trim <= TRIM_EXTENDED_MAX) return phase;
    uint8_t result = trim-TRIM_EXTENDED_MAX-1;
    if (result >= phase) result++;
    phase = result;
  }
  return 0;
}
#endif

#if defined(ROTARY_ENCODERS)
uint8_t getRotaryEncoderFlightPhase(uint8_t idx)
{
  uint8_t phase = mixerCurrentFlightMode;
  for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
    if (phase == 0) return 0;
    int16_t value = flightModeAddress(phase)->rotaryEncoders[idx];
    if (value <= ROTARY_ENCODER_MAX) return phase;
    uint8_t result = value-ROTARY_ENCODER_MAX-1;
    if (result >= phase) result++;
    phase = result;
  }
  return 0;
}

int16_t getRotaryEncoder(uint8_t idx)
{
  return flightModeAddress(getRotaryEncoderFlightPhase(idx))->rotaryEncoders[idx];
}

void incRotaryEncoder(uint8_t idx, int8_t inc)
{
  g_rotenc[idx] += inc;
  int16_t *value = &(flightModeAddress(getRotaryEncoderFlightPhase(idx))->rotaryEncoders[idx]);
  *value = limit((int16_t)-1024, (int16_t)(*value + (inc * 8)), (int16_t)+1024);
  storageDirty(EE_MODEL);
}
#endif

#if defined(GVARS)

#if defined(PCBSTD)
  #define SET_GVAR_VALUE(idx, phase, value) \
    (GVAR_VALUE(idx, phase) = value, storageDirty(EE_MODEL))
#else
  #define SET_GVAR_VALUE(idx, phase, value) \
    GVAR_VALUE(idx, phase) = value; \
    storageDirty(EE_MODEL); \
    if (g_model.gvars[idx].popup) { \
      gvarLastChanged = idx; \
      gvarDisplayTimer = GVAR_DISPLAY_TIME; \
    }
#endif

#if defined(PCBSTD)
int16_t getGVarFieldValue(int16_t x, int16_t min, int16_t max)
{
  if (GV_IS_GV_VALUE(x, min, max)) {
    int8_t idx = GV_INDEX_CALCULATION(x, max);
    int8_t mul = 1;

    if (idx < 0) {
      idx = -1-idx;
      mul = -1;
    }

    x = GVAR_VALUE(idx, -1) * mul;
  }

  return limit(min, x, max);
}

void setGVarValue(uint8_t idx, int8_t value)
{
  if (GVAR_VALUE(idx, -1) != value) {
    SET_GVAR_VALUE(idx, -1, value);
  }
}
#else
uint8_t gvarDisplayTimer = 0;
uint8_t gvarLastChanged = 0;

uint8_t getGVarFlightMode(uint8_t fm, uint8_t gv) // TODO change params order to be consistent!
{
  for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
    if (fm == 0) return 0;
    int16_t val = GVAR_VALUE(gv, fm);
    if (val <= GVAR_MAX) return fm;
    uint8_t result = val-GVAR_MAX-1;
    if (result >= fm) result++;
    fm = result;
  }
  return 0;
}

int16_t getGVarValue(int8_t gv, int8_t fm)
{
  int8_t mul = 1;
  if (gv < 0) {
    gv = -1-gv;
    mul = -1;
  }
  return GVAR_VALUE(gv, getGVarFlightMode(fm, gv)) * mul;
}

int32_t getGVarValuePrec1(int8_t gv, int8_t fm)
{
  int8_t mul;
  uint8_t prec = g_model.gvars[gv].prec;
  if (prec == 0)
    mul = 10;
  else
    mul = 1;

  if (gv < 0) {
    gv = -1-gv;
    mul = -mul;
  }
  return GVAR_VALUE(gv, getGVarFlightMode(fm, gv)) * mul;
}

void setGVarValue(uint8_t gv, int16_t value, int8_t fm)
{
  fm = getGVarFlightMode(fm, gv);
  if (GVAR_VALUE(gv, fm) != value) {
    SET_GVAR_VALUE(gv, fm, value);
  }
}

int16_t getGVarFieldValue(int16_t val, int16_t min, int16_t max, int8_t fm)
{
  if (GV_IS_GV_VALUE(val, min, max)) {
    int8_t gv = GV_INDEX_CALCULATION(val, max);
    val = getGVarValue(gv, fm);
  }
  return limit(min, val, max);
}

int32_t getGVarFieldValuePrec1(int16_t val, int16_t min, int16_t max, int8_t fm)
{
  if (GV_IS_GV_VALUE(val, min, max)) {
    int8_t gv = GV_INDEX_CALCULATION(val, max);
    val = getGVarValuePrec1(gv, fm);
  }
  else {
    val *= 10;
  }
  return limit<int>(min*10, val, max*10);
}
#endif

#endif

#if defined(CPUARM)
getvalue_t convert16bitsTelemValue(source_t channel, ls_telemetry_value_t value)
{
  return value;
}

getvalue_t convert8bitsTelemValue(source_t channel, ls_telemetry_value_t value)
{
  return value;
}

#if defined(FRSKY)
ls_telemetry_value_t minTelemValue(source_t channel)
{
  return 0;
}

ls_telemetry_value_t maxTelemValue(source_t channel)
{
  return 30000;
}
#endif

ls_telemetry_value_t max8bitsTelemValue(source_t channel)
{
  return 30000;
}

#elif defined(FRSKY)

/*
ls_telemetry_value_t minTelemValue(uint8_t channel)
{
  switch (channel) {
    case TELEM_TIMER1:
    case TELEM_TIMER2:
      return -3600;
    case TELEM_ALT:
    case TELEM_MIN_ALT:
    case TELEM_MAX_ALT:
    case TELEM_GPSALT:
      return -500;
    case TELEM_T1:
    case TELEM_MAX_T1:
    case TELEM_T2:
    case TELEM_MAX_T2:
      return -30;
    case TELEM_ACCx:
    case TELEM_ACCy:
    case TELEM_ACCz:
      return -1000;
    case TELEM_VSPEED:
      return -3000;
    default:
      return 0;
  }
}
*/
ls_telemetry_value_t maxTelemValue(uint8_t channel)
{
  switch (channel) {
    case TELEM_FUEL:
    case TELEM_RSSI_TX:
    case TELEM_RSSI_RX:
      return 100;
    case TELEM_HDG:
      return 180;
    default:
      return 255;
  }
}
#endif

#if !defined(CPUARM)
getvalue_t convert8bitsTelemValue(uint8_t channel, ls_telemetry_value_t value)
{
  getvalue_t result;
  switch (channel) {
    case TELEM_TIMER1:
    case TELEM_TIMER2:
      result = value * 5;
      break;
#if defined(FRSKY)
    case TELEM_ALT:
    case TELEM_GPSALT:
    case TELEM_MAX_ALT:
    case TELEM_MIN_ALT:
      result = value * 8 - 500;
      break;
    case TELEM_RPM:
    case TELEM_MAX_RPM:
      result = value * 50;
      break;
    case TELEM_T1:
    case TELEM_T2:
    case TELEM_MAX_T1:
    case TELEM_MAX_T2:
      result = (getvalue_t)value - 30;
      break;
    case TELEM_CELL:
    case TELEM_HDG:
    case TELEM_SPEED:
    case TELEM_MAX_SPEED:
      result = value * 2;
      break;
    case TELEM_ASPEED:
    case TELEM_MAX_ASPEED:
      result = value * 20;
      break;
    case TELEM_DIST:
    case TELEM_MAX_DIST:
      result = value * 8;
      break;
    case TELEM_CURRENT:
    case TELEM_POWER:
    case TELEM_MAX_CURRENT:
    case TELEM_MAX_POWER:
      result = value * 5;
      break;
    case TELEM_CONSUMPTION:
      result = value * 100;
      break;
    case TELEM_VSPEED:
      result = ((getvalue_t)value - 125) * 10;
      break;
#endif
    default:
      result = value;
      break;
  }
  return result;
}
#endif

#define INAC_STICKS_SHIFT   6
#define INAC_SWITCHES_SHIFT 8
bool inputsMoved()
{
  uint8_t sum = 0;
  for (uint8_t i=0; i<NUM_STICKS+NUM_POTS; i++)
    sum += anaIn(i) >> INAC_STICKS_SHIFT;
  for (uint8_t i=0; i<NUM_SWITCHES; i++)
    sum += getValue(MIXSRC_FIRST_SWITCH+i) >> INAC_SWITCHES_SHIFT;

  if (abs((int8_t)(sum-inactivity.sum)) > 1) {
    inactivity.sum = sum;
    return true;
  }
  else {
    return false;
  }
}

void checkBacklight()
{
  static uint8_t tmr10ms ;

#if defined(PCBSTD) && defined(ROTARY_ENCODER_NAVIGATION)
  rotencPoll();
#endif

  uint8_t x = g_blinkTmr10ms;
  if (tmr10ms != x) {
    tmr10ms = x;
    if (inputsMoved()) {
      inactivity.counter = 0;
      if (g_eeGeneral.backlightMode & e_backlight_mode_sticks)
        backlightOn();
    }

    bool backlightOn = (g_eeGeneral.backlightMode == e_backlight_mode_on || lightOffCounter || isFunctionActive(FUNCTION_BACKLIGHT));
    if (flashCounter) backlightOn = !backlightOn;
    if (backlightOn)
      BACKLIGHT_ON();
    else
      BACKLIGHT_OFF();

#if defined(PCBSTD) && defined(VOICE) && !defined(SIMU)
    Voice.voice_process() ;
#endif
  }
}

#if defined(PCBFLAMENCO)
void checkUsbChip()
{
  uint8_t reg = i2cReadBQ24195(0x00);
  if (reg & 0x80) {
    i2cWriteBQ24195(0x00, reg & 0x7F);
  }
}
#endif

void doLoopCommonActions()
{
  checkBacklight();

#if defined(PCBFLAMENCO)
  checkUsbChip();
#endif
}

void backlightOn()
{
  lightOffCounter = ((uint16_t)g_eeGeneral.lightAutoOff*250) << 1;
}

#if MENUS_LOCK == 1
bool readonly = true;
bool readonlyUnlocked()
{
  if (readonly) {
    POPUP_WARNING(STR_MODS_FORBIDDEN);
    return false;
  }
  else {
    return true;
  }
}
#endif

#if defined(SPLASH)
void doSplash()
{
#if defined(PWR_BUTTON_DELAY)
  bool refresh = false;
#endif

  if (SPLASH_NEEDED()) {
    drawSplash();

#if !defined(CPUARM)
    AUDIO_HELLO();
#endif

#if defined(PCBSTD)
    lcdSetContrast();
#elif defined(PCBSKY9X)
    tmr10ms_t curTime = get_tmr10ms() + 10;
    uint8_t contrast = 10;
    lcdSetRefVolt(contrast);
#endif

    getADC(); // init ADC array

    inputsMoved();

    tmr10ms_t tgtime = get_tmr10ms() + SPLASH_TIMEOUT;

    while (tgtime > get_tmr10ms()) {
#if defined(SIMU)
      SIMU_SLEEP(1);
#elif defined(CPUARM)
      CoTickDelay(1);
#endif

      getADC();

#if defined(FSPLASH)
      // Splash is forced, we can't skip it
      if (!(g_eeGeneral.splashMode & 0x04)) {
#endif

      if (keyDown() || inputsMoved()) return;

#if defined(FSPLASH)
      }
#endif

#if defined(PWR_BUTTON_DELAY)
      uint32_t pwr_check = pwrCheck();
      if (pwr_check == e_power_off) {
        break;
      }
      else if (pwr_check == e_power_press) {
        refresh = true;
      }
      else if (pwr_check == e_power_on && refresh) {
        drawSplash();
        refresh = false;
      }
#else
      if (pwrCheck() == e_power_off) {
        return;
      }
#endif

#if defined(PCBSKY9X)
      if (curTime < get_tmr10ms()) {
        curTime += 10;
        if (contrast < g_eeGeneral.contrast) {
          contrast += 1;
          lcdSetRefVolt(contrast);
        }
      }
#endif

      doLoopCommonActions();
    }
  }
}
#else
#define Splash()
#define doSplash()
#endif

#if defined(PCBTARANIS)
void checkFailsafe()
{
  for (int i=0; i<NUM_MODULES; i++) {
    if (IS_MODULE_XJT(i)) {
      ModuleData & moduleData = g_model.moduleData[i];
      if (HAS_RF_PROTOCOL_FAILSAFE(moduleData.rfProtocol) && moduleData.failsafeMode == FAILSAFE_NOT_SET) {
        ALERT(STR_FAILSAFEWARN, STR_NO_FAILSAFE, AU_ERROR);
        break;
      }
    }
  }
}
#else
#define checkFailsafe()
#endif

#if defined(GUI)
void checkAll()
{
#if defined(EEPROM_RLC)
  checkLowEEPROM();
#endif

#if defined(MODULE_ALWAYS_SEND_PULSES)
  startupWarningState = STARTUP_WARNING_THROTTLE;
#else
  if (g_eeGeneral.chkSum == evalChkSum()) {
    checkTHR();
  }
  checkSwitches();
  checkFailsafe();
#endif

#if defined(CPUARM)
  if (g_model.displayChecklist && modelHasNotes()) {
    pushModelNotes();
  }
#endif

#if defined(CPUARM)
  if (!clearKeyEvents()) {
    displayPopup(STR_KEYSTUCK);
    tmr10ms_t tgtime = get_tmr10ms() + 500;
    while (tgtime != get_tmr10ms()) {
#if defined(SIMU)
      SIMU_SLEEP(1);
#elif defined(CPUARM)
      CoTickDelay(1);
#endif
      wdt_reset();
    }
  }
#else    // #if defined(CPUARM)
  clearKeyEvents();
#endif   // #if defined(CPUARM)

  START_SILENCE_PERIOD();
}
#endif // GUI

#if defined(MODULE_ALWAYS_SEND_PULSES)
void checkStartupWarnings()
{
  if (startupWarningState < STARTUP_WARNING_DONE) {
    if (startupWarningState == STARTUP_WARNING_THROTTLE)
      checkTHR();
    else
      checkSwitches();
  }
}
#endif

#if defined(EEPROM_RLC)
void checkLowEEPROM()
{
  if (g_eeGeneral.disableMemoryWarning) return;
  if (EeFsGetFree() < 100) {
    ALERT(STR_STORAGE_WARNING, STR_EEPROMLOWMEM, AU_ERROR);
  }
}
#endif

void checkTHR()
{
  uint8_t thrchn = ((g_model.thrTraceSrc==0) || (g_model.thrTraceSrc>NUM_POTS)) ? THR_STICK : g_model.thrTraceSrc+NUM_STICKS-1;
  // throttle channel is either the stick according stick mode (already handled in evalInputs)
  // or P1 to P3;
  // in case an output channel is choosen as throttle source (thrTraceSrc>NUM_POTS) we assume the throttle stick is the input
  // no other information available at the moment, and good enough to my option (otherwise too much exceptions...)

#if defined(MODULE_ALWAYS_SEND_PULSES)
  int16_t v = calibratedStick[thrchn];
  if (v<=THRCHK_DEADBAND-1024 || g_model.disableThrottleWarning || pwrCheck()==e_power_off || keyDown()) {
    startupWarningState = STARTUP_WARNING_THROTTLE+1;
  }
  else {
    calibratedStick[thrchn] = -1024;
#if !defined(VIRTUALINPUTS)
    if (thrchn < NUM_STICKS) {
      rawAnas[thrchn] = anas[thrchn] = calibratedStick[thrchn];
    }
#endif
    MESSAGE(STR_THROTTLEWARN, STR_THROTTLENOTIDLE, STR_PRESSANYKEYTOSKIP, AU_THROTTLE_ALERT);
  }
#else
  if (g_model.disableThrottleWarning) {
    return;
  }

  getADC();

  evalInputs(e_perout_mode_notrainer); // let do evalInputs do the job

  int16_t v = calibratedStick[thrchn];
  if (v <= THRCHK_DEADBAND-1024) {
    return; // prevent warning if throttle input OK
  }

  // first - display warning; also deletes inputs if any have been before
  LED_ERROR_BEGIN();
  MESSAGE(STR_THROTTLEWARN, STR_THROTTLENOTIDLE, STR_PRESSANYKEYTOSKIP, AU_THROTTLE_ALERT);

#if defined(PWR_BUTTON_DELAY)
  bool refresh = false;
#endif

  while (1) {

    SIMU_SLEEP(1);

    getADC();

    evalInputs(e_perout_mode_notrainer); // let do evalInputs do the job

    v = calibratedStick[thrchn];

#if defined(PWR_BUTTON_DELAY)
    uint32_t pwr_check = pwrCheck();
    if (pwr_check == e_power_off) {
      break;
    }
    else if (pwr_check == e_power_press) {
      refresh = true;
    }
    else if (pwr_check == e_power_on && refresh) {
      MESSAGE(STR_THROTTLEWARN, STR_THROTTLENOTIDLE, STR_PRESSANYKEYTOSKIP, AU_NONE);
      refresh = false;
    }
#else
    if (pwrCheck() == e_power_off) {
      break;
    }
#endif

    if (keyDown() || v <= THRCHK_DEADBAND-1024) {
      break;
    }

    doLoopCommonActions();

    wdt_reset();
  }
#endif

  LED_ERROR_END();
}

void checkAlarm() // added by Gohst
{
  if (g_eeGeneral.disableAlarmWarning) {
    return;
  }

  if (IS_SOUND_OFF()) {
    ALERT(STR_ALARMSWARN, STR_ALARMSDISABLED, AU_ERROR);
  }
}

void alert(const pm_char * t, const pm_char * s MESSAGE_SOUND_ARG)
{
  LED_ERROR_BEGIN();

  MESSAGE(t, s, STR_PRESSANYKEY, sound);

#if defined(PWR_BUTTON_DELAY)
  bool refresh = false;
#endif

  while(1) {
    SIMU_SLEEP(1);

    if (keyDown()) break; // wait for key release

    doLoopCommonActions();

    wdt_reset();

#if defined(PWR_BUTTON_DELAY)
    uint32_t pwr_check = pwrCheck();
    if (pwr_check == e_power_off) {
      boardOff();
    }
    else if (pwr_check == e_power_press) {
      refresh = true;
    }
    else if (pwr_check == e_power_on && refresh) {
      MESSAGE(t, s, STR_PRESSANYKEY, AU_NONE);
      refresh = false;
    }
#else
    if (pwrCheck() == e_power_off) {
      boardOff(); // turn power off now
    }
#endif
  }

  LED_ERROR_END();
}

#if defined(GVARS)
int8_t trimGvar[NUM_STICKS+NUM_AUX_TRIMS] = { -1, -1, -1, -1 };
#endif

#if defined(CPUARM)
void checkTrims()
{
  evt_t event = getEvent(true);
  if (event && !IS_KEY_BREAK(event)) {
    int8_t k = EVT_KEY_MASK(event) - TRM_BASE;
#else
uint8_t checkTrim(uint8_t event)
{
  int8_t k = EVT_KEY_MASK(event) - TRM_BASE;
  if (k>=0 && k<8 && !IS_KEY_BREAK(event)) {
#endif
    // LH_DWN LH_UP LV_DWN LV_UP RV_DWN RV_UP RH_DWN RH_UP
    uint8_t idx = CONVERT_MODE((uint8_t)k/2);
    uint8_t phase;
    int before;
    bool thro;

#if defined(CPUARM)
    trimsDisplayTimer = 200; // 2 seconds
    trimsDisplayMask |= (1<<idx);
#endif

#if defined(GVARS)
    if (TRIM_REUSED(idx)) {
#if defined(PCBSTD)
      phase = 0;
#else
      phase = getGVarFlightMode(mixerCurrentFlightMode, trimGvar[idx]);
#endif
      before = GVAR_VALUE(trimGvar[idx], phase);
      thro = false;
    }
    else {
      phase = getTrimFlightPhase(mixerCurrentFlightMode, idx);
#if defined(VIRTUALINPUTS)
      before = getTrimValue(phase, idx);
#else
      before = getRawTrimValue(phase, idx);
#endif
      thro = (idx==THR_STICK && g_model.thrTrim);
    }
#else
    phase = getTrimFlightPhase(mixerCurrentFlightMode, idx);
#if defined(VIRTUALINPUTS)
    before = getTrimValue(phase, idx);
#else
    before = getRawTrimValue(phase, idx);
#endif
    thro = (idx==THR_STICK && g_model.thrTrim);
#endif
    int8_t trimInc = g_model.trimInc + 1;
    int8_t v = (trimInc==-1) ? min(32, abs(before)/4+1) : (1 << trimInc); // TODO flash saving if (trimInc < 0)
    if (thro) v = 4; // if throttle trim and trim trottle then step=4
    int16_t after = (k&1) ? before + v : before - v;   // positive = k&1
    bool beepTrim = false;

    if (!thro && after==0 && before!=0) {
      beepTrim = true;
      AUDIO_TRIM_MIDDLE();
      pauseEvents(event);
    }
    else if (before>TRIM_MIN && after<=TRIM_MIN) {
      beepTrim = true;
      AUDIO_TRIM_MIN();
      killEvents(event);
    }
    else if (before<TRIM_MAX && after>=TRIM_MAX) {
      beepTrim = true;
      AUDIO_TRIM_MAX();
      killEvents(event);
    }

    if ((before<after && after>TRIM_MAX) || (before>after && after<TRIM_MIN)) {
      if (!g_model.extendedTrims || TRIM_REUSED(idx)) after = before;
    }

    if (after < TRIM_EXTENDED_MIN) {
      after = TRIM_EXTENDED_MIN;
    }
    if (after > TRIM_EXTENDED_MAX) {
      after = TRIM_EXTENDED_MAX;
    }

#if defined(GVARS)
    if (TRIM_REUSED(idx)) {
      SET_GVAR_VALUE(trimGvar[idx], phase, after);
    }
    else
#endif
    {
#if defined(VIRTUALINPUTS)
      if (!setTrimValue(phase, idx, after)) {
        // we don't play a beep, so we exit now the function
        return;
      }
#else
      setTrimValue(phase, idx, after);
#endif
    }

    if (!beepTrim) {
      AUDIO_TRIM_PRESS(after);
    }

#if !defined(CPUARM)
    return 0;
#endif
  }
#if !defined(CPUARM)
  return event;
#endif
}

#if !defined(SIMU)
uint16_t s_anaFilt[NUMBER_ANALOG];
#endif

#if defined(SIMU)
uint16_t BandGap = 225;
#elif defined(CPUM2560)
// #define STARTADCONV (ADCSRA  = (1<<ADEN) | (1<<ADPS0) | (1<<ADPS1) | (1<<ADPS2) | (1<<ADSC) | (1 << ADIE))
// G: Note that the above would have set the ADC prescaler to 128, equating to
// 125KHz sample rate. We now sample at 500KHz, with oversampling and other
// filtering options to produce 11-bit results.
uint16_t BandGap = 2040;
#elif defined(PCBSTD)
uint16_t BandGap;
#endif

#if defined(JITTER_MEASURE)
JitterMeter<uint16_t> rawJitter[NUMBER_ANALOG];
JitterMeter<uint16_t> avgJitter[NUMBER_ANALOG];
tmr10ms_t jitterResetTime = 0;
#endif

#if defined(VIRTUALINPUTS)
  #define JITTER_FILTER_STRENGTH  4         // tune this value, bigger value - more filtering (range: 1-5) (see explanation below)
  #define ANALOG_SCALE            1         // tune this value, bigger value - more filtering (range: 0-1) (see explanation below)

  #define JITTER_ALPHA            (1<<JITTER_FILTER_STRENGTH)
  #define ANALOG_MULTIPLIER       (1<<ANALOG_SCALE)
  #define ANA_FILT(chan)          (s_anaFilt[chan] / (JITTER_ALPHA * ANALOG_MULTIPLIER))
  #if (JITTER_ALPHA * ANALOG_MULTIPLIER > 32)
    #error "JITTER_FILTER_STRENGTH and ANALOG_SCALE are too big, their summ should be <= 5 !!!"
  #endif
#else
  #define ANALOG_SCALE            0
  #define JITTER_ALPHA            1
  #define ANALOG_MULTIPLIER       1
  #define ANA_FILT(chan)          (s_anaFilt[chan])
#endif


#if !defined(SIMU)
uint16_t anaIn(uint8_t chan)
{
#if defined(VIRTUALINPUTS)
  #if defined(PCBSKY9X) && !defined(REVA)
    static const uint8_t crossAna[]={1,5,7,0,4,6,2,3};
    chan = crossAna[chan];
  #endif
  return ANA_FILT(chan);
#elif defined(PCBSKY9X) && !defined(REVA)
  static const uint8_t crossAna[]={1,5,7,0,4,6,2,3};
  if (chan == TX_CURRENT) {
    return Current_analogue ;
  }
  volatile uint16_t *p = &s_anaFilt[pgm_read_byte(crossAna+chan)];
  return *p;
#else
#if defined(TELEMETRY_MOD_14051) || defined(TELEMETRY_MOD_14051_SWAPPED)
  static const pm_char crossAna[] PROGMEM = {3,1,2,0,4,5,6,0/* shouldn't be used */,TX_VOLTAGE};
#else
  static const pm_char crossAna[] PROGMEM = {3,1,2,0,4,5,6,7};
#endif
#if defined(FRSKY_STICKS)
  volatile uint16_t temp = s_anaFilt[pgm_read_byte(crossAna+chan)];  // volatile saves here 40 bytes; maybe removed for newer AVR when available
  if (chan < NUM_STICKS && (g_eeGeneral.stickReverse & (1 << chan))) {
    temp = 2048 - temp;
  }
  return temp;
#else
  volatile uint16_t *p = &s_anaFilt[pgm_read_byte(crossAna+chan)];
  return *p;
#endif
#endif
}

#if defined(CPUARM)
void getADC()
{
#if defined(JITTER_MEASURE)
  if (JITTER_MEASURE_ACTIVE() && jitterResetTime < get_tmr10ms()) {
    // reset jitter measurement every second
    for (uint32_t x=0; x<NUMBER_ANALOG; x++) {
      rawJitter[x].reset();
      avgJitter[x].reset();
    }
    jitterResetTime = get_tmr10ms() + 100;  //every second
  }
#endif

  DEBUG_TIMER_START(debugTimerAdcRead);
  adcRead();
  DEBUG_TIMER_STOP(debugTimerAdcRead);

  for (uint8_t x=0; x<NUMBER_ANALOG; x++) {
    uint16_t v = getAnalogValue(x) >> (1 - ANALOG_SCALE);

#if defined(VIRTUALINPUTS)
    // Jitter filter:
    //    * pass trough any big change directly
    //    * for small change use Modified moving average (MMA) filter
    //
    // Explanation:
    //
    // Normal MMA filter has this formula:
    //            <out> = ((ALPHA-1)*<out> + <in>)/ALPHA
    //
    // If calculation is done this way with integer arithmetics, then any small change in
    // input signal is lost. One way to combat that, is to rearrange the formula somewhat,
    // to store a more precise (larger) number between iterations. The basic idea is to
    // store undivided value between iterations. Therefore an new variable <filtered> is
    // used. The new formula becomes:
    //           <filtered> = <filtered> - <filtered>/ALPHA + <in>
    //           <out> = <filtered>/ALPHA  (use only when out is needed)
    //
    // The above formula with a maximum allowed ALPHA value (we are limited by
    // the 16 bit s_anaFilt[]) was tested on the radio. The resulting signal still had
    // some jitter (a value of 1 was observed). The jitter might be bigger on other
    // radios.
    //
    // So another idea is to use larger input values for filtering. So instead of using
    // input in a range from 0 to 2047, we use twice larger number (temp[x] is divided less)
    //
    // This also means that ALPHA must be lowered (remember 16 bit limit), but test results
    // have proved that this kind of filtering gives better results. So the recommended values
    // for filter are:
    //     JITTER_FILTER_STRENGTH  4
    //     ANALOG_SCALE            1
    //
    // Variables mapping:
    //   * <in> = v
    //   * <out> = s_anaFilt[x]
    uint16_t previous = s_anaFilt[x] / JITTER_ALPHA;
    uint16_t diff = (v > previous) ? (v - previous) : (previous - v);
    if (!g_eeGeneral.jitterFilter && diff < (10*ANALOG_MULTIPLIER)) { // g_eeGeneral.jitterFilter is inverted, 0 - active
      // apply jitter filter
      s_anaFilt[x] = (s_anaFilt[x] - previous) + v;
    }
    else
#endif
    {
    	//use unfiltered value
      s_anaFilt[x] = v * JITTER_ALPHA;
    }

#if defined(JITTER_MEASURE)
    if (JITTER_MEASURE_ACTIVE()) {
      avgJitter[x].measure(ANA_FILT(x));
    }
#endif

#if defined(VIRTUALINPUTS)
    #define ANAFILT_MAX    (2 * RESX * JITTER_ALPHA * ANALOG_MULTIPLIER - 1)
    StepsCalibData * calib = (StepsCalibData *) &g_eeGeneral.calib[x];
    if (IS_POT_MULTIPOS(x) && IS_MULTIPOS_CALIBRATED(calib)) {
      // TODO: consider adding another low pass filter to eliminate multipos switching glitches
      uint8_t vShifted = ANA_FILT(x) >> 4;
      s_anaFilt[x] = ANAFILT_MAX;
      for (uint32_t i=0; i<calib->count; i++) {
        if (vShifted < calib->steps[i]) {
          s_anaFilt[x] = (i * ANAFILT_MAX) / calib->count;
          break;
        }
      }
    }
#endif   // defined(VIRTUALINPUTS)
  }
}
#endif

#endif // SIMU

uint8_t g_vbat100mV = 0;
uint16_t lightOffCounter;
uint8_t flashCounter = 0;

uint16_t sessionTimer;
uint16_t s_timeCumThr;    // THR in 1/16 sec
uint16_t s_timeCum16ThrP; // THR% in 1/16 sec

uint8_t  trimsCheckTimer = 0;

#if defined(CPUARM)
uint8_t trimsDisplayTimer = 0;
uint8_t trimsDisplayMask = 0;
#endif

void flightReset(uint8_t check)
{
  // we don't reset the whole audio here (the hello.wav would be cut, if a prompt is queued before FlightReset, it should be played)
  // TODO check if the vario / background music are stopped correctly if switching to a model which doesn't have these functions enabled

  if (!IS_MANUAL_RESET_TIMER(0)) {
    timerReset(0);
  }

#if TIMERS > 1
  if (!IS_MANUAL_RESET_TIMER(1)) {
    timerReset(1);
  }
#endif

#if TIMERS > 2
  if (!IS_MANUAL_RESET_TIMER(2)) {
    timerReset(2);
  }
#endif

#if defined(FRSKY)
  telemetryReset();
#endif

  s_mixer_first_run_done = false;

  START_SILENCE_PERIOD();

  RESET_THR_TRACE();

  logicalSwitchesReset();

  if (check) {
    checkAll();
  }
}

#if defined(THRTRACE)
uint8_t  s_traceBuf[MAXTRACE];
uint16_t s_traceWr;
uint8_t  s_cnt_10s;
uint16_t s_cnt_samples_thr_10s;
uint16_t s_sum_samples_thr_10s;
#endif

void evalTrims()
{
  uint8_t phase = mixerCurrentFlightMode;
  for (uint8_t i=0; i<NUM_STICKS+NUM_AUX_TRIMS; i++) {
    // do trim -> throttle trim if applicable
    int16_t trim = getTrimValue(phase, i);
#if !defined(VIRTUALINPUTS)
    if (i==THR_STICK && g_model.thrTrim) {
      int16_t trimMin = g_model.extendedTrims ? TRIM_EXTENDED_MIN : TRIM_MIN;
      trim = (((g_model.throttleReversed)?(int32_t)(trim+trimMin):(int32_t)(trim-trimMin)) * (RESX-anas[i])) >> (RESX_SHIFT+1);
    }
#endif
    if (trimsCheckTimer > 0) {
      trim = 0;
    }

    trims[i] = trim*2;
  }
}

#if !defined(PCBSTD)
uint8_t mSwitchDuration[1+NUM_ROTARY_ENCODERS] = { 0 };
#define CFN_PRESSLONG_DURATION   100
#endif


uint8_t s_mixer_first_run_done = false;

void doMixerCalculations()
{
  static tmr10ms_t lastTMR = 0;

  tmr10ms_t tmr10ms = get_tmr10ms();
  uint8_t tick10ms = (tmr10ms >= lastTMR ? tmr10ms - lastTMR : 1);
  // handle tick10ms overrun
  // correct overflow handling costs a lot of code; happens only each 11 min;
  // therefore forget the exact calculation and use only 1 instead; good compromise
  lastTMR = tmr10ms;

  DEBUG_TIMER_START(debugTimerGetAdc);
  getADC();
  DEBUG_TIMER_STOP(debugTimerGetAdc);

  DEBUG_TIMER_START(debugTimerGetSwitches);
  getSwitchesPosition(!s_mixer_first_run_done);
  DEBUG_TIMER_STOP(debugTimerGetSwitches);

#if defined(PCBSKY9X) && !defined(REVA) && !defined(SIMU)
  Current_analogue = (Current_analogue*31 + s_anaFilt[8] ) >> 5 ;
  if (Current_analogue > Current_max)
    Current_max = Current_analogue ;
#endif

#if !defined(CPUARM)
  adcPrepareBandgap();
#endif

  DEBUG_TIMER_START(debugTimerEvalMixes);
  evalMixes(tick10ms);
  DEBUG_TIMER_STOP(debugTimerEvalMixes);

#if !defined(CPUARM)
  // Bandgap has had plenty of time to settle...
  getADC_bandgap();
#endif

  DEBUG_TIMER_START(debugTimerMixes10ms);
  if (tick10ms) {

#if !defined(CPUM64) && !defined(ACCURAT_THROTTLE_TIMER)
    //  code cost is about 16 bytes for higher throttle accuracy for timer
    //  would not be noticable anyway, because all version up to this change had only 16 steps;
    //  now it has already 32  steps; this define would increase to 128 steps
    #define ACCURAT_THROTTLE_TIMER
#endif

    /* Throttle trace */
    int16_t val;

    if (g_model.thrTraceSrc > NUM_POTS) {
      uint8_t ch = g_model.thrTraceSrc-NUM_POTS-1;
      val = channelOutputs[ch];

      LimitData *lim = limitAddress(ch);
      int16_t gModelMax = LIMIT_MAX_RESX(lim);
      int16_t gModelMin = LIMIT_MIN_RESX(lim);

      if (lim->revert)
        val = -val + gModelMax;
      else
        val = val - gModelMin;

#if defined(PPM_LIMITS_SYMETRICAL)
      if (lim->symetrical) {
        val -= calc1000toRESX(lim->offset);
      }
#endif

      gModelMax -= gModelMin; // we compare difference between Max and Mix for recaling needed; Max and Min are shifted to 0 by default
      // usually max is 1024 min is -1024 --> max-min = 2048 full range

#ifdef ACCURAT_THROTTLE_TIMER
      if (gModelMax!=0 && gModelMax!=2048) val = (int32_t) (val << 11) / (gModelMax); // rescaling only needed if Min, Max differs
#else
      // @@@ open.20.fsguruh  optimized calculation; now *8 /8 instead of 10 base; (*16/16 already cause a overrun; unsigned calculation also not possible, because v may be negative)
      gModelMax+=255; // force rounding up --> gModelMax is bigger --> val is smaller
      gModelMax >>= (10-2);

      if (gModelMax!=0 && gModelMax!=8) {
        val = (val << 3) / gModelMax; // rescaling only needed if Min, Max differs
      }
#endif

      if (val<0) val=0;  // prevent val be negative, which would corrupt throttle trace and timers; could occur if safetyswitch is smaller than limits
    }
    else {
#if defined(VIRTUALINPUTS)
      val = RESX + calibratedStick[g_model.thrTraceSrc == 0 ? THR_STICK : g_model.thrTraceSrc+NUM_STICKS-1];
#else
      val = RESX + (g_model.thrTraceSrc == 0 ? rawAnas[THR_STICK] : calibratedStick[g_model.thrTraceSrc+NUM_STICKS-1]);
#endif
    }

#if defined(ACCURAT_THROTTLE_TIMER)
    val >>= (RESX_SHIFT-6); // calibrate it (resolution increased by factor 4)
#else
    val >>= (RESX_SHIFT-4); // calibrate it
#endif

    evalTimers(val, tick10ms);

    static uint8_t  s_cnt_100ms;
    static uint8_t  s_cnt_1s;
    static uint8_t  s_cnt_samples_thr_1s;
    static uint16_t s_sum_samples_thr_1s;

    s_cnt_samples_thr_1s++;
    s_sum_samples_thr_1s+=val;

    if ((s_cnt_100ms += tick10ms) >= 10) { // 0.1sec
      s_cnt_100ms -= 10;
      s_cnt_1s += 1;

      logicalSwitchesTimerTick();
      checkTrainerSignalWarning();

      if (s_cnt_1s >= 10) { // 1sec
        s_cnt_1s -= 10;
        sessionTimer += 1;

        struct t_inactivity *ptrInactivity = &inactivity;
        FORCE_INDIRECT(ptrInactivity) ;
        ptrInactivity->counter++;
        if ((((uint8_t)ptrInactivity->counter)&0x07)==0x01 && g_eeGeneral.inactivityTimer && g_vbat100mV>50 && ptrInactivity->counter > ((uint16_t)g_eeGeneral.inactivityTimer*60))
          AUDIO_INACTIVITY();

#if defined(AUDIO)
        if (mixWarning & 1) if ((sessionTimer&0x03)==0) AUDIO_MIX_WARNING(1);
        if (mixWarning & 2) if ((sessionTimer&0x03)==1) AUDIO_MIX_WARNING(2);
        if (mixWarning & 4) if ((sessionTimer&0x03)==2) AUDIO_MIX_WARNING(3);
#endif

#if defined(ACCURAT_THROTTLE_TIMER)
        val = s_sum_samples_thr_1s / s_cnt_samples_thr_1s;
        s_timeCum16ThrP += (val>>3);  // s_timeCum16ThrP would overrun if we would store throttle value with higher accuracy; therefore stay with 16 steps
        if (val) s_timeCumThr += 1;
        s_sum_samples_thr_1s>>=2;  // correct better accuracy now, because trace graph can show this information; in case thrtrace is not active, the compile should remove this
#else
        val = s_sum_samples_thr_1s / s_cnt_samples_thr_1s;
        s_timeCum16ThrP += (val>>1);
        if (val) s_timeCumThr += 1;
#endif

#if defined(THRTRACE)
        // throttle trace is done every 10 seconds; Tracebuffer is adjusted to screen size.
        // in case buffer runs out, it wraps around
        // resolution for y axis is only 32, therefore no higher value makes sense
        s_cnt_samples_thr_10s += s_cnt_samples_thr_1s;
        s_sum_samples_thr_10s += s_sum_samples_thr_1s;

        if (++s_cnt_10s >= 10) { // 10s
          s_cnt_10s -= 10;
          val = s_sum_samples_thr_10s / s_cnt_samples_thr_10s;
          s_sum_samples_thr_10s = 0;
          s_cnt_samples_thr_10s = 0;
          s_traceBuf[s_traceWr % MAXTRACE] = val;
          s_traceWr++;
        }
#endif

        s_cnt_samples_thr_1s = 0;
        s_sum_samples_thr_1s = 0;
      }
    }

#if defined(PXX) || defined(DSM2)
    static uint8_t countRangecheck = 0;
    for (uint8_t i=0; i<NUM_MODULES; ++i) {
      if (moduleFlag[i] != MODULE_NORMAL_MODE) {
        if (++countRangecheck >= 250) {
          countRangecheck = 0;
          AUDIO_PLAY(AU_SPECIAL_SOUND_CHEEP);
        }
      }
    }
#endif

#if defined(CPUARM)
    checkTrims();
#endif
  }
  DEBUG_TIMER_STOP(debugTimerMixes10ms);

  s_mixer_first_run_done = true;
}

#if defined(NAVIGATION_STICKS)
uint8_t StickScrollAllowed;
uint8_t StickScrollTimer;
static const pm_uint8_t rate[] PROGMEM = { 0, 0, 100, 40, 16, 7, 3, 1 } ;

uint8_t calcStickScroll( uint8_t index )
{
  uint8_t direction;
  int8_t value;

  if ( ( g_eeGeneral.stickMode & 1 ) == 0 )
    index ^= 3;

  value = calibratedStick[index] / 128;
  direction = value > 0 ? 0x80 : 0;
  if (value < 0)
    value = -value;                        // (abs)
  if (value > 7)
    value = 7;
  value = pgm_read_byte(rate+(uint8_t)value);
  if (value)
    StickScrollTimer = STICK_SCROLL_TIMEOUT;               // Seconds
  return value | direction;
}
#endif

void opentxStart()
{
#if defined(SIMU)
  if (main_thread_running == 2)
    return;
#endif

  bool display_alerts = (g_eeGeneral.chkSum == evalChkSum());

#if defined(GUI)
  if (display_alerts) {
    doSplash();
  }
#endif

#if defined(DEBUG_TRACE_BUFFER)
  trace_event(trace_start, 0x12345678);
#endif

#if defined(PCBSKY9X) && defined(SDCARD) && !defined(SIMU)
  for (int i=0; i<500 && !Card_initialized; i++) {
    CoTickDelay(1);  // 2ms
  }
#endif

#if defined(GUI)
  if (display_alerts) {
    checkAlarm();
    checkAll();
  }
  else {
    chainMenu(menuFirstCalib);
  }
#endif
}

#if defined(CPUARM) || defined(CPUM2560)
void opentxClose()
{
  AUDIO_BYE();

#if defined(FRSKY)
  // TODO needed? telemetryEnd();
#endif

#if defined(LUA)
  luaClose();
#endif

#if defined(SDCARD)
  closeLogs();
#endif

#if defined(HAPTIC)
  hapticOff();
#endif

  saveTimers();

#if defined(CPUARM)
  for (int i=0; i<MAX_SENSORS; i++) {
    TelemetrySensor & sensor = g_model.telemetrySensors[i];
    if (sensor.type == TELEM_TYPE_CALCULATED && sensor.persistent && sensor.persistentValue != telemetryItems[i].value) {
      sensor.persistentValue = telemetryItems[i].value;
      storageDirty(EE_MODEL);
    }
  }
#endif

#if defined(PCBSKY9X)
  uint32_t mAhUsed = g_eeGeneral.mAhUsed + Current_used * (488 + g_eeGeneral.txCurrentCalibration) / 8192 / 36;
  if (g_eeGeneral.mAhUsed != mAhUsed) {
    g_eeGeneral.mAhUsed = mAhUsed;
  }
#endif

#if defined(PCBTARANIS)
  if (g_model.potsWarnMode == POTS_WARN_AUTO) {
    for (int i=0; i<NUM_POTS; i++) {
      if (!(g_model.potsWarnEnabled & (1 << i))) {
        SAVE_POT_POSITION(i);
      }
    }
    storageDirty(EE_MODEL);
  }
#endif

#if !defined(PCBTARANIS) && !defined(COLORLCD)
  if (storageDirtyMsk & EE_MODEL) {
    displayPopup(STR_SAVEMODEL);
  }
#endif

  g_eeGeneral.unexpectedShutdown = 0;

  storageDirty(EE_GENERAL);
  storageCheck(true);

#if defined(CPUARM)
  while (IS_PLAYING(ID_PLAY_BYE)) {
    CoTickDelay(10);
  }

  CoTickDelay(50);
#endif

#if defined(SDCARD)
  sdDone();
#endif
}
#endif

#if defined(NAVIGATION_STICKS)
uint8_t getSticksNavigationEvent()
{
  uint8_t evt = 0;
  if (StickScrollAllowed) {
    if ( StickScrollTimer ) {
      static uint8_t repeater;
      uint8_t direction;
      uint8_t value;

      if ( repeater < 128 )
      {
        repeater += 1;
      }
      value = calcStickScroll( 2 );
      direction = value & 0x80;
      value &= 0x7F;
      if ( value )
      {
        if ( repeater > value )
        {
          repeater = 0;
          if ( evt == 0 )
          {
            if ( direction )
            {
              evt = EVT_KEY_FIRST(KEY_UP);
            }
            else
            {
              evt = EVT_KEY_FIRST(KEY_DOWN);
            }
          }
        }
      }
      else
      {
        value = calcStickScroll( 3 );
        direction = value & 0x80;
        value &= 0x7F;
        if ( value )
        {
          if ( repeater > value )
          {
            repeater = 0;
            if ( evt == 0 )
            {
              if ( direction )
              {
                evt = EVT_KEY_FIRST(KEY_RIGHT);
              }
              else
              {
                evt = EVT_KEY_FIRST(KEY_LEFT);
              }
            }
          }
        }
      }
    }
  }
  else {
    StickScrollTimer = 0;          // Seconds
  }
  StickScrollAllowed = 1 ;
  return evt;
}
#endif

#if !defined(CPUARM)
void checkBattery()
{
  static uint8_t counter = 0;
#if defined(GUI) && !defined(COLORLCD)
  // TODO not the right menu I think ...
  if (menuHandlers[menuLevel] == menuGeneralDiagAna) {
    g_vbat100mV = 0;
    counter = 0;
  }
#endif
  if (counter-- == 0) {
    counter = 10;
    int32_t instant_vbat = anaIn(TX_VOLTAGE);
#if defined(CPUM2560)
    instant_vbat = (instant_vbat*1112 + instant_vbat*g_eeGeneral.txVoltageCalibration + (BandGap<<2)) / (BandGap<<3);
#else
    instant_vbat = (instant_vbat*16 + instant_vbat*g_eeGeneral.txVoltageCalibration/8) / BandGap;
#endif

    static uint8_t  s_batCheck;
    static uint16_t s_batSum;

#if defined(VOICE)
    s_batCheck += 8;
#else
    s_batCheck += 32;
#endif

    s_batSum += instant_vbat;

    if (g_vbat100mV == 0) {
      g_vbat100mV = instant_vbat;
      s_batSum = 0;
      s_batCheck = 0;
    }
#if defined(VOICE)
    else if (!(s_batCheck & 0x3f)) {
#else
    else if (s_batCheck == 0) {
#endif
      g_vbat100mV = s_batSum / 8;
      s_batSum = 0;
#if defined(VOICE)
      if (s_batCheck != 0) {
        // no alarms
      }
      else
#endif
      if (IS_TXBATT_WARNING() && g_vbat100mV>50) {
        AUDIO_TX_BATTERY_LOW();
      }
    }
  }
}
#endif // #if !defined(CPUARM)


#if !defined(SIMU) && !defined(CPUARM)

volatile uint8_t g_tmr16KHz; //continuous timer 16ms (16MHz/1024/256) -- 8-bit counter overflow
ISR(TIMER_16KHZ_VECT, ISR_NOBLOCK)
{
  g_tmr16KHz++; // gruvin: Not 16KHz. Overflows occur at 61.035Hz (1/256th of 15.625KHz)
                // to give *16.384ms* intervals. Kind of matters for accuracy elsewhere. ;)
                // g_tmr16KHz is used to software-construct a 16-bit timer
                // from TIMER-0 (8-bit). See getTmr16KHz, below.
}

uint16_t getTmr16KHz()
{
  while(1){
    uint8_t hb  = g_tmr16KHz;
    uint8_t lb  = COUNTER_16KHZ;
    if(hb-g_tmr16KHz==0) return (hb<<8)|lb;
  }
}

#if defined(PCBSTD) && (defined(AUDIO) || defined(VOICE))
// Clocks every 128 uS
ISR(TIMER_AUDIO_VECT, ISR_NOBLOCK)
{
  cli();
  PAUSE_AUDIO_INTERRUPT(); // stop reentrance
  sei();

#if defined(AUDIO)
  AUDIO_DRIVER();
#endif

#if defined(VOICE)
  VOICE_DRIVER();
#endif

  cli();
  RESUME_AUDIO_INTERRUPT();
  sei();
}
#endif

// Clocks every 10ms
ISR(TIMER_10MS_VECT, ISR_NOBLOCK)
{
  // without correction we are 0,16% too fast; that mean in one hour we are 5,76Sek too fast; we do not like that
  static uint8_t accuracyWarble; // because 16M / 1024 / 100 = 156.25. we need to correct the fault; no start value needed

#if defined(AUDIO)
  AUDIO_HEARTBEAT();
#endif

#if defined(BUZZER)
  BUZZER_HEARTBEAT();
#endif

#if defined(HAPTIC)
  HAPTIC_HEARTBEAT();
#endif

  per10ms();

  uint8_t bump = (!(++accuracyWarble & 0x03)) ? 157 : 156;
  TIMER_10MS_COMPVAL += bump;
}

// Timer3 used for PPM_IN pulse width capture. Counter running at 16MHz / 8 = 2MHz
// equating to one count every half millisecond. (2 counts = 1ms). Control channel
// count delta values thus can range from about 1600 to 4400 counts (800us to 2200us),
// corresponding to a PPM signal in the range 0.8ms to 2.2ms (1.5ms at center).
// (The timer is free-running and is thus not reset to zero at each capture interval.)
ISR(TIMER3_CAPT_vect) // G: High frequency noise can cause stack overflo with ISR_NOBLOCK
{
  uint16_t capture=ICR3;

  // Prevent rentrance for this IRQ only
  PAUSE_PPMIN_INTERRUPT();
  sei(); // enable other interrupts

  captureTrainerPulses(capture);

  cli(); // disable other interrupts for stack pops before this function's RETI
  RESUME_PPMIN_INTERRUPT();
}
#endif

#if defined(DSM2_SERIAL) && !defined(CPUARM)
FORCEINLINE void DSM2_USART_vect()
{
  UDR0 = *((uint16_t*)pulses2MHzRPtr); // transmit next byte

  pulses2MHzRPtr += sizeof(uint16_t);

  if (pulses2MHzRPtr == pulses2MHzWPtr) { // if reached end of DSM2 data buffer ...
    UCSRB_N(TLM_USART) &= ~(1 << UDRIE_N(TLM_USART)); // disable UDRE interrupt
  }
}
#endif

#if !defined(SIMU) && !defined(CPUARM)

#if defined (FRSKY)

FORCEINLINE void FRSKY_USART_vect()
{
  if (frskyTxBufferCount > 0) {
    UDR_N(TLM_USART) = frskyTxBuffer[--frskyTxBufferCount];
  }
  else {
    UCSRB_N(TLM_USART) &= ~(1 << UDRIE_N(TLM_USART)); // disable UDRE interrupt
  }
}

// USART0/1 Transmit Data Register Emtpy ISR
ISR(USART_UDRE_vect_N(TLM_USART))
{
#if defined(FRSKY) && defined(DSM2_SERIAL)
  if (IS_DSM2_PROTOCOL(g_model.protocol)) { // TODO not s_current_protocol?
    DSM2_USART_vect();
  }
  else {
    FRSKY_USART_vect();
  }
#elif defined(FRSKY)
  FRSKY_USART_vect();
#else
  DSM2_USART_vect();
#endif
}
#endif
#endif

#if defined(VIRTUALINPUTS)
  #define INSTANT_TRIM_MARGIN 10 /* around 1% */
#else
  #define INSTANT_TRIM_MARGIN 15 /* around 1.5% */
#endif

void instantTrim()
{
#if defined(VIRTUALINPUTS)
  int16_t  anas_0[NUM_INPUTS];
  evalInputs(e_perout_mode_notrainer | e_perout_mode_nosticks);
  memcpy(anas_0, anas, sizeof(anas_0));
#endif

  evalInputs(e_perout_mode_notrainer);

  for (uint8_t stick=0; stick<NUM_STICKS; stick++) {
    if (stick!=THR_STICK) {
      // don't instant trim the throttle stick
      uint8_t trim_phase = getTrimFlightPhase(mixerCurrentFlightMode, stick);
#if defined(VIRTUALINPUTS)
      int16_t delta = 0;
      for (int e=0; e<MAX_EXPOS; e++) {
        ExpoData * ed = expoAddress(e);
        if (!EXPO_VALID(ed)) break; // end of list
        if (ed->srcRaw-MIXSRC_Rud == stick) {
          delta = anas[ed->chn] - anas_0[ed->chn];
          break;
        }
      }
#else
      int16_t delta = anas[stick];
#endif
      if (abs(delta) >= INSTANT_TRIM_MARGIN) {
        int16_t trim = limit<int16_t>(TRIM_EXTENDED_MIN, (delta + trims[stick]) / 2, TRIM_EXTENDED_MAX);
        setTrimValue(trim_phase, stick, trim);
      }
    }
  }

  storageDirty(EE_MODEL);
  AUDIO_WARNING2();
}

void copySticksToOffset(uint8_t ch)
{
  pauseMixerCalculations();
  int32_t zero = (int32_t)channelOutputs[ch];

  evalFlightModeMixes(e_perout_mode_nosticks+e_perout_mode_notrainer, 0);
  int32_t val = chans[ch];
  LimitData *ld = limitAddress(ch);
  limit_min_max_t lim = LIMIT_MIN(ld);
  if (val < 0) {
    val = -val;
    lim = LIMIT_MIN(ld);
  }
#if defined(CPUARM)
  zero = (zero*256000 - val*lim) / (1024*256-val);
#else
  zero = (zero*25600 - val*lim) / (26214-val);
#endif
  ld->offset = (ld->revert ? -zero : zero);
  resumeMixerCalculations();
  storageDirty(EE_MODEL);
}

void copyTrimsToOffset(uint8_t ch)
{
  int16_t zero;

  pauseMixerCalculations();

  evalFlightModeMixes(e_perout_mode_noinput, 0); // do output loop - zero input sticks and trims
  zero = applyLimits(ch, chans[ch]);

  evalFlightModeMixes(e_perout_mode_noinput-e_perout_mode_notrims, 0); // do output loop - only trims

  int16_t output = applyLimits(ch, chans[ch]) - zero;
  int16_t v = g_model.limitData[ch].offset;
  if (g_model.limitData[ch].revert) output = -output;
#if defined(CPUARM)
  v += (output * 125) / 128;
#else
  v += output;
#endif
  g_model.limitData[ch].offset = limit((int16_t)-1000, (int16_t)v, (int16_t)1000); // make sure the offset doesn't go haywire

  resumeMixerCalculations();
  storageDirty(EE_MODEL);
}

void moveTrimsToOffsets() // copy state of 3 primary to subtrim
{
  int16_t zeros[NUM_CHNOUT];

  pauseMixerCalculations();

  evalFlightModeMixes(e_perout_mode_noinput, 0); // do output loop - zero input sticks and trims
  for (uint8_t i=0; i<NUM_CHNOUT; i++) {
    zeros[i] = applyLimits(i, chans[i]);
  }

  evalFlightModeMixes(e_perout_mode_noinput-e_perout_mode_notrims, 0); // do output loop - only trims

  for (uint8_t i=0; i<NUM_CHNOUT; i++) {
    int16_t output = applyLimits(i, chans[i]) - zeros[i];
    int16_t v = g_model.limitData[i].offset;
    if (g_model.limitData[i].revert) output = -output;
#if defined(CPUARM)
    v += (output * 125) / 128;
#else
    v += output;
#endif
    g_model.limitData[i].offset = limit((int16_t)-1000, (int16_t)v, (int16_t)1000); // make sure the offset doesn't go haywire
  }

  // reset all trims, except throttle (if throttle trim)
  for (uint8_t i=0; i<NUM_STICKS+NUM_AUX_TRIMS; i++) {
    if (i!=THR_STICK || !g_model.thrTrim) {
      int16_t original_trim = getTrimValue(mixerCurrentFlightMode, i);
      for (uint8_t phase=0; phase<MAX_FLIGHT_MODES; phase++) {
#if defined(VIRTUALINPUTS)
        trim_t trim = getRawTrimValue(phase, i);
        if (trim.mode / 2 == phase)
          setTrimValue(phase, i, trim.value - original_trim);
#else
        trim_t trim = getRawTrimValue(phase, i);
        if (trim <= TRIM_EXTENDED_MAX)
          setTrimValue(phase, i, trim - original_trim);
#endif
      }
    }
  }

  resumeMixerCalculations();

  storageDirty(EE_MODEL);
  AUDIO_WARNING2();
}

#if defined(ROTARY_ENCODERS)
  volatile rotenc_t g_rotenc[ROTARY_ENCODERS] = {0};
#elif defined(ROTARY_ENCODER_NAVIGATION)
  volatile rotenc_t g_rotenc[1] = {0};
#endif

#if !defined(CPUARM) && !defined(SIMU)
extern unsigned char __bss_end ;
#define STACKPTR     _SFR_IO16(0x3D)
void stackPaint()
{
  // Init Stack while interrupts are disabled
  unsigned char *p ;
  unsigned char *q ;

  p = (unsigned char *) STACKPTR ;
  q = &__bss_end ;
  p -= 2 ;
  while ( p > q ) {
    *p-- = 0x55 ;
  }
}

uint16_t stackAvailable()
{
  unsigned char *p ;

  p = &__bss_end + 1 ;
  while ( *p++ == 0x55 );
  return p - &__bss_end ;
}
#endif

#if defined(CPUM2560)
  #define OPENTX_INIT_ARGS const uint8_t mcusr
#elif defined(PCBSTD)
  #define OPENTX_INIT_ARGS const uint8_t mcusr
#else
  #define OPENTX_INIT_ARGS
#endif

void opentxInit(OPENTX_INIT_ARGS)
{
#if defined(DEBUG) && defined(USB_SERIAL)
  // CoTickDelay(5000); // 10s
#endif

  TRACE("opentxInit()");

#if defined(GUI)
  menuHandlers[0] = menuMainView;
  #if MENUS_LOCK != 2/*no menus*/
    menuHandlers[1] = menuModelSelect;
  #endif
#endif

#if defined(RTCLOCK) && !defined(COPROCESSOR)
  rtcInit();    // RTC must be initialized before rambackupRestore() is called
#endif

#if !defined(EEPROM)
  if (!UNEXPECTED_SHUTDOWN()) {
    sdInit();
  }
#endif

#if defined(COLORLCD)
  topbar = new Topbar(&g_model.topbarData);
  luaInit();
#endif

#if defined(RAMBACKUP)
  if (UNEXPECTED_SHUTDOWN()) {
    rambackupRestore();
  }
  else {
    storageReadAll();
  }
#else
  storageEraseAll(true);
  storageReadAll();
#endif

#if defined(CPUARM)
  if (UNEXPECTED_SHUTDOWN()) {
    unexpectedShutdown = 1;
  }
#endif

#if defined(PCBTARANIS)
  BACKLIGHT_ON();
#endif

#if MENUS_LOCK == 1
  getMovedSwitch();
  if (TRIMS_PRESSED() && g_eeGeneral.switchUnlockStates==switches_states) {
    readonly = false;
  }
#endif

#if defined(VOICE)
  setScaledVolume(g_eeGeneral.speakerVolume+VOLUME_LEVEL_DEF);
#endif

#if defined(CPUARM)
  audioQueue.start();
  setBacklight(g_eeGeneral.backlightBright);
#endif

#if defined(PCBSKY9X)
  // Set ADC gains here
  setSticksGain(g_eeGeneral.sticksGain);
#endif

#if defined(BLUETOOTH)
  btInit();
#endif

#if defined(COLORLCD)
  loadTheme();
  loadFontCache();
#endif

  if (g_eeGeneral.backlightMode != e_backlight_mode_off) backlightOn(); // on Tx start turn the light on

  if (UNEXPECTED_SHUTDOWN()) {
#if !defined(CPUARM)
    // is done above on ARM
    unexpectedShutdown = 1;
#endif
  }
  else {
    opentxStart();
  }

#if defined(CPUARM) || defined(CPUM2560)
  if (!g_eeGeneral.unexpectedShutdown) {
    g_eeGeneral.unexpectedShutdown = 1;
    storageDirty(EE_GENERAL);
  }
#endif

#if defined(GUI)
  lcdSetContrast();
#endif
  backlightOn();

#if defined(PCBTARANIS) || defined(PCBFLAMENCO)
  serial2Init(g_eeGeneral.serial2Mode, MODEL_TELEMETRY_PROTOCOL());
#endif

#if defined(PCBSKY9X) && !defined(SIMU)
  init_trainer_capture();
#endif

#if !defined(CPUARM)
  doMixerCalculations();
#endif

  startPulses();

  wdt_enable(WDT_500MS);
}

#if defined(SIMU)
void * simuMain(void *)
#else
int main()
#endif
{
  // G: The WDT remains active after a WDT reset -- at maximum clock speed. So it's
  // important to disable it before commencing with system initialisation (or
  // we could put a bunch more wdt_reset()s in. But I don't like that approach
  // during boot up.)
#if defined(CPUM2560) || defined(CPUM2561)
  uint8_t mcusr = MCUSR; // save the WDT (etc) flags
  MCUSR = 0; // must be zeroed before disabling the WDT
  MCUCR = 0x80 ;   // Disable JTAG port that can interfere with POT3
  MCUCR = 0x80 ;   // Must be done twice
#elif defined(PCBSTD)
  uint8_t mcusr = MCUCSR;
  MCUCSR = 0x80 ;   // Disable JTAG port that can interfere with POT3
  MCUCSR = 0x80 ;   // Must be done twice
#endif
#if defined(PCBTARANIS)
  g_eeGeneral.contrast = 30;
#endif
  wdt_disable();

  boardInit();

#if defined(GUI) && !defined(PCBTARANIS) && !defined(PCBFLAMENCO) && !defined(PCBHORUS)
  // TODO remove this
  lcdInit();
#endif

#if !defined(SIMU)
  stackPaint();
#endif

#if defined(GUI) && !defined(PCBTARANIS)
  // lcdSetRefVolt(25);
#endif

#if defined(PCBTARANIS) || defined(PCBHORUS)
  drawSplash();
#endif

  sei(); // interrupts needed now

#if !defined(CPUARM) && defined(FRSKY) && !defined(DSM2_SERIAL)
  telemetryInit();
#endif

#if defined(DSM2_SERIAL) && !defined(FRSKY)
  DSM2_Init();
#endif

#ifdef JETI
  JETI_Init();
#endif

#ifdef ARDUPILOT
  ARDUPILOT_Init();
#endif

#ifdef NMEA
  NMEA_Init();
#endif

#ifdef MAVLINK
  MAVLINK_Init();
#endif

#ifdef MENU_ROTARY_SW
  init_rotary_sw();
#endif

#if defined(CPUARM)
  tasksStart();
#else
  opentxInit(mcusr);
#if defined(CPUM2560)
  uint8_t shutdown_state = 0;
#endif

#if defined(PCBFLAMENCO)
  // TODO not here it's an ARM board ... menuEntryTime = get_tmr10ms() - 200;
#endif

  while (1) {
#if defined(CPUM2560)
    if ((shutdown_state=pwrCheck()) > e_power_trainer)
      break;
#endif
#if defined(SIMU)
    sleep(5/*ms*/);
    if (main_thread_running == 0)
      return 0;
#endif

    perMain();

    if (heartbeat == HEART_WDT_CHECK) {
      wdt_reset();
      heartbeat = 0;
    }
  }
#endif

#if defined(CPUM2560)
  // Time to switch off
  lcdClear();
  displayPopup(STR_SHUTDOWN);
  opentxClose();
  lcdClear() ;
  lcdRefresh() ;
  boardOff(); // Only turn power off if necessary
  wdt_disable();
  while(1); // never return from main() - there is no code to return back, if any delays occurs in physical power it does dead loop.
#endif

#if defined(SIMU)
  return NULL;
#endif
}

#if defined(PWR_BUTTON_DELAY)

uint32_t pwr_press_time = 0;

uint32_t pwrPressedDuration()
{
  if (pwr_press_time == 0) {
    return 0;
  }
  else {
    return get_tmr10ms() - pwr_press_time;
  }
}

uint32_t pwrCheck()
{
  enum PwrCheckState {
    PWR_CHECK_ON,
    PWR_CHECK_OFF,
    PWR_CHECK_PAUSED,
  };

  static uint8_t pwr_check_state = PWR_CHECK_ON;

  if (pwr_check_state == PWR_CHECK_OFF) {
    return e_power_off;
  }
  else if (pwrPressed()) {
    if (pwr_check_state == PWR_CHECK_PAUSED) {
      // nothing
    }
    else if (pwr_press_time == 0) {
      pwr_press_time = get_tmr10ms();
    }
    else {
      if (get_tmr10ms() - pwr_press_time > PWR_PRESS_SHUTDOWN) {
#if defined(SHUTDOWN_CONFIRMATION)
        while (1) {
          lcdRefreshWait();
          lcdClear();
          POPUP_CONFIRMATION("Confirm Shutdown");
          evt_t evt = getEvent(false);
          DISPLAY_WARNING(evt);
          lcdRefresh();
          if (warningResult == true) {
            pwr_check_state = PWR_CHECK_OFF;
            return e_power_off;
          }
          else if (!warningText) {
            // shutdown has been cancelled
            pwr_check_state = PWR_CHECK_PAUSED;
            return e_power_on;
          }
        }
#else
        haptic.play(15, 3, PLAY_NOW);
        pwr_check_state = PWR_CHECK_OFF;
        return e_power_off;
#endif
      }
      else {
        drawShutdownBitmap(pwrPressedDuration());
        return e_power_press;
      }
    }
  }
  else {
    pwr_check_state = PWR_CHECK_ON;
    pwr_press_time = 0;
  }

  return e_power_on;
}
#endif
