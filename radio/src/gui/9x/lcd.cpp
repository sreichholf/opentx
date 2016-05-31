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

display_t displayBuf[DISPLAY_BUFFER_SIZE];

void lcdClear()
{
  memset(displayBuf, 0, DISPLAY_BUFFER_SIZE);
}

coord_t lcdLastPos;
coord_t lcdNextPos;

#if defined(CPUARM)
void lcdPutPattern(coord_t x, coord_t y, const uint8_t * pattern, uint8_t width, uint8_t height, LcdFlags flags)
{
  bool blink = false;
  bool inv = false;
  if (flags & BLINK) {
    if (BLINK_ON_PHASE) {
      if (flags & INVERS)
        inv = true;
      else {
        blink = true;
      }
    }
  }
  else if (flags & INVERS) {
    inv = true;
  }

  uint8_t lines = (height+7)/8;
  assert(lines <= 5);

  for (int8_t i=0; i<width+2; i++) {
    if (x<LCD_W) {
      uint8_t b[5] = { 0 };
      if (i==0) {
        if (x==0 || !inv) {
          lcdNextPos++;
          continue;
        }
        else {
          // we need to work on the previous x when INVERS
          x--;
        }
      }
      else if (i<=width) {
        uint8_t skip = true;
        for (uint8_t j=0; j<lines; j++) {
          b[j] = pgm_read_byte(pattern++); /*top byte*/
          if (b[j] != 0xff) {
            skip = false;
          }
        }
        if (skip) {
          if (flags & FIXEDWIDTH) {
            for (uint8_t j=0; j<lines; j++) {
              b[j] = 0;
            }
          }
          else {
            continue;
          }
        }
        if ((flags & CONDENSED) && i==2) {
          /*condense the letter by skipping column 3 */
          continue;
        }
      }

      for (int8_t j=-1; j<=height; j++) {
        bool plot;
        if (j < 0 || ((j == height) && !(FONTSIZE(flags) == SMLSIZE))) {
          plot = false;
          if (height >= 12) continue;
          if (j<0 && !inv) continue;
          if (y+j < 0) continue;
        }
        else {
          uint8_t line = (j / 8);
          uint8_t pixel = (j % 8);
          plot = b[line] & (1 << pixel);
        }
        if (inv) plot = !plot;
        if (!blink) {
          if (flags & VERTICAL)
            lcdDrawPoint(y+j, LCD_H-x, plot ? FORCE : ERASE);
          else
            lcdDrawPoint(x, y+j, plot ? FORCE : ERASE);
        }
      }
    }

    x++;
    lcdNextPos++;
  }
}

void lcdDrawChar(coord_t x, coord_t y, const unsigned char c, LcdFlags flags)
{
  const pm_uchar * q;

  lcdNextPos = x-1;

#if !defined(BOOT)
  uint32_t fontsize = FONTSIZE(flags);
  unsigned char c_remapped = 0;

  if (fontsize == DBLSIZE || (flags&BOLD)) {
    // To save space only some DBLSIZE and BOLD chars are available
    // c has to be remapped. All non existing chars mapped to 0 (space)
    if (c>=',' && c<=':')
      c_remapped = c - ',' + 1;
    else if (c>='A' && c<='Z')
      c_remapped = c - 'A' + 16;
    else if (c>='a' && c<='z')
      c_remapped = c - 'a' + 42;
    else if (c=='_')
      c_remapped = 4;
    else if (c!=' ')
      flags &= ~BOLD;
  }

  if (fontsize == DBLSIZE) {
    if (c >= 0xC0) {
      q = &font_10x14_extra[((uint16_t)(c-0xC0))*20];
    }
    else {
      if (c >= 128)
        c_remapped = c - 60;
      q = &font_10x14[((uint16_t)c_remapped)*20];
    }
    lcdPutPattern(x, y, q, 10, 16, flags);
  }
  else if (fontsize == XXLSIZE) {
    q = &font_22x38_num[((uint16_t)c-'0'+5)*110];
    lcdPutPattern(x, y, q, 22, 38, flags);
  }
  else if (fontsize == MIDSIZE) {
    q = &font_8x10[((uint16_t)c-0x20)*16];
    lcdPutPattern(x, y, q, 8, 12, flags);
  }
  else if (fontsize == SMLSIZE) {
    q = (c < 0xc0 ? &font_4x6[(c-0x20)*5] : &font_4x6_extra[(c-0xc0)*5]);
    lcdPutPattern(x, y, q, 5, 6, flags);
  }
  else if (fontsize == TINSIZE) {
    q = &font_3x5[((uint16_t)c-0x20)*3];
    lcdPutPattern(x, y, q, 3, 5, flags);
  }
#if defined(BOLD_FONT)
  else if (flags & BOLD) {
    q = &font_5x7_B[c_remapped*5];
    lcdPutPattern(x, y, q, 5, 7, flags);
  }
#endif
  else
#endif
  {
#if !defined(BOOT)
    q = (c < 0xC0) ? &font_5x7[(c-0x20)*5] : &font_5x7_extra[(c-0xC0)*5];
#else
    q = &font_5x7[(c-0x20)*5];
#endif
    lcdPutPattern(x, y, q, 5, 7, flags);
  }
}
#endif

void lcdDrawChar(coord_t x, coord_t y, const unsigned char c)
{
  lcdDrawChar(x, y, c, 0);
}

void lcdDrawSizedText(coord_t x, coord_t y, const pm_char * s, uint8_t len, LcdFlags flags)
{
  const coord_t orig_x = x;
#if defined(CPUARM)
  const uint8_t orig_len = len;
  uint32_t fontsize = FONTSIZE(flags);
#endif
  bool setx = false;
  while (len--) {
    unsigned char c;
    switch (flags & (BSS+ZCHAR)) {
      case BSS:
        c = *s;
        break;
#if !defined(BOOT)
      case ZCHAR:
        c = idx2char(*s);
        break;
#endif
      default:
        c = pgm_read_byte(s);
        break;
    }

    if (setx) {
      x = c;
      setx = false;
    }
    else if (!c) {
      break;
    }
    else if (c >= 0x20) {
      lcdDrawChar(x, y, c, flags);
      x = lcdNextPos;
    }
    else if (c == 0x1F) {  //X-coord prefix
      setx = true;
    }
    else if (c == 0x1E) {  //NEWLINE
#if defined(CPUARM)
      len = orig_len;
#endif
      x = orig_x;
      y += FH;
#if defined(CPUARM)      
      if (fontsize == DBLSIZE)
        y += FH;
      else if (fontsize == MIDSIZE)
        y += 4;
      else if (fontsize == SMLSIZE)
        y--;
#endif
      if (y >= LCD_H) break;
    }
#if defined(CPUARM)      
    else if (c == 0x1D) {  // TAB
      x |= 0x3F;
      x += 1;
    }
#endif
    else {
      x += (c*FW/2); // EXTENDED SPACE
    }
    s++;
  }
  lcdLastPos = x;
  lcdNextPos = x;
#if defined(CPUARM)
  if (fontsize == MIDSIZE)
    lcdLastPos += 1;
#endif
}

void lcdDrawSizedText(coord_t x, coord_t y, const pm_char * s, uint8_t len)
{
  lcdDrawSizedText(x, y, s, len, 0);
}

void lcdDrawText(coord_t x, coord_t y, const pm_char * s, LcdFlags flags)
{
  lcdDrawSizedText(x, y, s, 255, flags);
}

void lcdDrawText(coord_t x, coord_t y, const pm_char * s)
{
  lcdDrawText(x, y, s, 0);
}

void lcd_putsLeft(coord_t y, const pm_char * s)
{
  lcdDrawText(0, y, s);
}

#if !defined(BOOT)
void lcdDrawTextAtIndex(coord_t x, coord_t y, const pm_char * s,uint8_t idx, LcdFlags flags)
{
  uint8_t length;
  length = pgm_read_byte(s++);
  lcdDrawSizedText(x, y, s+length*idx, length, flags & ~(BSS|ZCHAR));
}

#if defined(CPUARM)
void lcdDrawHexNumber(coord_t x, coord_t y, uint32_t val, LcdFlags flags)
{
  x += FWNUM*4+1;
  for (int i=0; i<4; i++) {
    x -= FWNUM;
    char c = val & 0xf;
    c = c>9 ? c+'A'-10 : c+'0';
    lcdDrawChar(x, y, c, flags|(c>='A' ? CONDENSED : 0));
    val >>= 4;
  }
}
#else
void lcdDrawHexNumber(coord_t x, coord_t y, uint16_t val)
{
  x += FWNUM*4+1;
  for(int i=0; i<4; i++) {
    x -= FWNUM;
    char c = val & 0xf;
    c = c>9 ? c+'A'-10 : c+'0';
    lcdDrawChar(x, y, c, c>='A' ? CONDENSED : 0);
    val >>= 4;
  }
}
#endif

void lcdDraw8bitsNumber(coord_t x, coord_t y, int8_t val)
{
  lcdDrawNumber(x, y, val);
}

void lcdDrawNumber(coord_t x, coord_t y, lcdint_t val, LcdFlags flags)
{
  lcdDrawNumber(x, y, val, flags, 0);
}

void lcdDrawNumber(coord_t x, coord_t y, lcdint_t val, LcdFlags flags, uint8_t len)
{
  uint8_t fw = FWNUM;
  int8_t mode = MODE(flags);
  flags &= ~LEADING0;
#if defined(CPUARM)
  uint32_t fontsize = FONTSIZE(flags);
  bool dblsize = (fontsize == DBLSIZE);
  bool xxlsize = (fontsize == XXLSIZE);
  bool midsize = (fontsize == MIDSIZE);
  bool smlsize = (fontsize == SMLSIZE);
  bool tinsize = (fontsize == TINSIZE);
#else
  bool dblsize = flags & DBLSIZE;
  #define xxlsize 0
  #define midsize 0
  #define smlsize 0
  #define tinsize 0
#endif

  bool neg = false;
  if (flags & UNSIGN) {
    flags -= UNSIGN;
  }
  else if (val < 0) {
    neg = true;
    val = -val;
  }

  coord_t xn = 0;
  uint8_t ln = 2;

  if (mode != MODE(LEADING0)) {
    len = 1;
    lcduint_t tmp = ((lcduint_t)val) / 10;
    while (tmp) {
      len++;
      tmp /= 10;
    }
    if (len <= mode) {
      len = mode + 1;
    }
  }

  if (dblsize) {
    fw += FWNUM;
  }
  else if (xxlsize) {
    fw += 4*FWNUM-1;
  }
  else if (midsize) {
    fw += FWNUM-3;
  }
  else if (tinsize) {
    fw -= 1;
  }
  else {
    if (flags & LEFT) {
      if (mode > 0)
        x += 2;
    }
#if defined(BOLD_FONT) && !defined(CPUM64) || defined(EXTSTD)
    if (flags & BOLD) fw += 1;
#endif
  }

  if (flags & LEFT) {
    x += len * fw;
    if (neg) {
      x += ((xxlsize|dblsize|midsize) ? 7 : FWNUM);
    }
  }

  lcdLastPos = x;
  x -= fw;
  if (dblsize) x++;

  for (uint8_t i=1; i<=len; i++) {
    div_t qr = div((lcduint_t)val, 10);
    char c = qr.rem + '0';
    LcdFlags f = flags;
    if (dblsize) {
      if (c=='1' && i==len && xn>x+10) { x+=1; }
      if ((lcduint_t)val >= 1000) { x+=FWNUM; f &= ~DBLSIZE; }
    }
    lcdDrawChar(x, y, c, f);
    if (mode == i) {
      flags &= ~PREC2; // TODO not needed but removes 20bytes, could be improved for sure, check asm
      if (dblsize) {
        xn = x - 2;
        if (c>='2' && c<='3') ln++;
        uint8_t tn = (qr.quot % 10);
        if (tn==2 || tn==4) {
          if (c=='4') {
            xn++;
          }
          else {
            xn--;
            ln++;
          }
        }
      }
      else if (xxlsize) {
        x -= 17;
        lcdDrawChar(x+2, y, '.', f);
      }
      else if (midsize) {
        x -= 3;
        xn = x;
      }
      else if (smlsize) {
        x -= 2;
        lcdDrawPoint(x, y+5);
        if ((flags&INVERS) && ((~flags & BLINK) || BLINK_ON_PHASE)) {
          lcdDrawSolidVerticalLine(x, y-1, 8);
        }
      }
      else if (tinsize) {
        x--;
        lcdDrawPoint(x-1, y+4);
        if ((flags&INVERS) && ((~flags & BLINK) || BLINK_ON_PHASE)) {
          lcdDrawSolidVerticalLine(x-1, y-1, 7);
        }
        x--;
      }
      else {
        x -= 2;
        lcdDrawChar(x, y, '.', f);
      }
    }
    if (dblsize && (lcduint_t)val >= 1000 && (lcduint_t)val < 10000) x-=2;
    val = qr.quot;
    x -= fw;
#if defined(BOLD_FONT) && !defined(CPUM64) || defined(EXTSTD)
    if (i==len && (flags & BOLD)) x += 1;
#endif
  }

  if (xn) {
    if (midsize) {
      if ((flags&INVERS) && ((~flags & BLINK) || BLINK_ON_PHASE)) {
        lcdDrawSolidVerticalLine(xn, y, 12);
        lcdDrawSolidVerticalLine(xn+1, y, 12);
      }
      lcdDrawSolidHorizontalLine(xn, y+9, 2);
      lcdDrawSolidHorizontalLine(xn, y+10, 2);
    }
    else {
      // TODO needed on CPUAVR? y &= ~0x07;
      lcdDrawFilledRect(xn, y+2*FH-3, ln, 2);
    }
  }
  if (neg) lcdDrawChar(x, y, '-', flags);
}
#endif

void lcdDrawSolidHorizontalLine(coord_t x, coord_t y, coord_t w, LcdFlags att)
{
  lcdDrawHorizontalLine(x, y, w, 0xff, att);
}

#if defined(CPUARM) && !defined(BOOT)
void lcdDrawLine(coord_t x1, coord_t y1, coord_t x2, coord_t y2, uint8_t pat, LcdFlags att)
{
  int dx = x2-x1;      /* the horizontal distance of the line */
  int dy = y2-y1;      /* the vertical distance of the line */
  int dxabs = abs(dx);
  int dyabs = abs(dy);
  int sdx = sgn(dx);
  int sdy = sgn(dy);
  int x = dyabs>>1;
  int y = dxabs>>1;
  int px = x1;
  int py = y1;

  if (dxabs >= dyabs) {
    /* the line is more horizontal than vertical */
    for (int i=0; i<=dxabs; i++) {
      y += dyabs;
      if (y>=dxabs) {
        y -= dxabs;
        py += sdy;
      }
      if ((1<<(px%8)) & pat) {
        lcdDrawPoint(px, py, att);
      }
      px += sdx;
    }
  }
  else {
    /* the line is more vertical than horizontal */
    for (int i=0; i<=dyabs; i++) {
      x += dxabs;
      if (x >= dyabs) {
        x -= dyabs;
        px += sdx;
      }
      if ((1<<(py%8)) & pat) {
        lcdDrawPoint(px, py, att);
      }
      py += sdy;
    }
  }
}
#endif

void lcdDrawSolidVerticalLine(coord_t x, scoord_t y, scoord_t h)
{
  lcdDrawVerticalLine(x, y, h, SOLID);
}

void lcdDrawRect(coord_t x, coord_t y, coord_t w, coord_t h, uint8_t pat, LcdFlags att)
{
  lcdDrawVerticalLine(x, y, h, pat);
  lcdDrawVerticalLine(x+w-1, y, h, pat);
  if (~att & ROUND) { x+=1; w-=2; }
  lcdDrawHorizontalLine(x, y+h-1, w, pat);
  lcdDrawHorizontalLine(x, y, w, pat);
}

#if !defined(BOOT)
void lcdDrawFilledRect(coord_t x, scoord_t y, coord_t w, coord_t h, uint8_t pat, LcdFlags att)
{
#if defined(CPUM64)
  for (scoord_t i=y; i<y+h; i++) {
    lcdDrawHorizontalLine(x, i, w, pat, att);
    pat = (pat >> 1) + ((pat & 1) << 7);
  }
#else
  for (scoord_t i=y; i<y+h; i++) {
    if ((att&ROUND) && (i==y || i==y+h-1))
      lcdDrawHorizontalLine(x+1, i, w-2, pat, att);
    else
      lcdDrawHorizontalLine(x, i, w, pat, att);
    pat = (pat >> 1) + ((pat & 1) << 7);
  }
#endif
}

void lcdDrawTelemetryTopBar()
{
  putsModelName(0, 0, g_model.header.name, g_eeGeneral.currModel, 0);
  uint8_t att = (IS_TXBATT_WARNING() ? BLINK : 0);
  putsVBat(14*FW,0,att);
  if (g_model.timers[0].mode) {
    att = (timersStates[0].val<0 ? BLINK : 0);
    putsTimer(17*FW+5*FWNUM+1, 0, timersStates[0].val, att, att);
  }
  lcdInvertLine(0);
}

#if defined(CPUARM) && defined(RTCLOCK)
void putsRtcTime(coord_t x, coord_t y, LcdFlags att)
{
  putsTimer(x, y, getValue(MIXSRC_TX_TIME), att, att);
}
#endif

void putsTimer(coord_t x, coord_t y, putstime_t tme, LcdFlags att, LcdFlags att2)
{
  div_t qr;

  if (!(att & LEFT)) {
    if (att & DBLSIZE)
      x -= 5*(2*FWNUM)-4;
    else if (att & MIDSIZE)
      x -= 5*8-4;
    else
      x -= 5*FWNUM+1;
  }

  if (tme < 0) {
    lcdDrawChar(x - ((att & DBLSIZE) ? FW+2 : ((att & MIDSIZE) ? FW+0 : FWNUM)), y, '-', att);
    tme = -tme;
  }

  qr = div(tme, 60);

#if defined(CPUARM)
  char separator = ':';
  if (tme >= 3600) {
    qr = div(qr.quot, 60);
    separator = CHR_HOUR;
  }
#else
#define separator ':'
#endif
  lcdDrawNumber(x, y, qr.quot, att|LEADING0|LEFT, 2);
#if defined(CPUARM)
  if (separator == CHR_HOUR)
    att &= ~DBLSIZE;
#endif
#if defined(CPUARM) && defined(RTCLOCK)
  if (att&TIMEBLINK)
    lcdDrawChar(lcdLastPos, y, separator, BLINK);
  else
#endif
  lcdDrawChar(lcdLastPos, y, separator, att&att2);
  lcdDrawNumber(lcdNextPos, y, qr.rem, att2|LEADING0|LEFT, 2);
}

// TODO to be optimized with putsValueWithUnit
void putsVolts(coord_t x, coord_t y, uint16_t volts, LcdFlags att)
{
  lcdDrawNumber(x, y, (int16_t)volts, (~NO_UNIT) & (att | ((att&PREC2)==PREC2 ? 0 : PREC1)));
  if (~att & NO_UNIT) lcdDrawChar(lcdLastPos, y, 'V', att);
}

void putsVBat(coord_t x, coord_t y, LcdFlags att)
{
  putsVolts(x, y, g_vbat100mV, att);
}

void drawStringWithIndex(coord_t x, coord_t y, const pm_char *str, uint8_t idx, LcdFlags att)
{
  lcdDrawText(x, y, str, att & ~LEADING0);
  lcdDrawNumber(lcdNextPos, y, idx, att|LEFT, 2);
}

void putsMixerSource(coord_t x, coord_t y, uint8_t idx, LcdFlags att)
{
  if (idx == MIXSRC_NONE) {
    lcdDrawTextAtIndex(x, y, STR_VSRCRAW, 0, att); // TODO macro
  }
  else if (idx <= MIXSRC_LAST_INPUT) {
    lcdDrawChar(x+2, y+1, CHR_INPUT, TINSIZE);
    lcdDrawFilledRect(x, y, 7, 7);
    if (ZEXIST(g_model.inputNames[idx-MIXSRC_FIRST_INPUT]))
      lcdDrawSizedText(x+8, y, g_model.inputNames[idx-MIXSRC_FIRST_INPUT], LEN_INPUT_NAME, ZCHAR|att);
    else
      lcdDrawNumber(x+8, y, idx, att|LEADING0|LEFT, 2);
  }

  else if (idx <= MIXSRC_LAST_LUA) {
    div_t qr = div(idx-MIXSRC_FIRST_LUA, MAX_SCRIPT_OUTPUTS);
#if defined(LUA_MODEL_SCRIPTS)
    if (qr.quot < MAX_SCRIPTS && qr.rem < scriptInputsOutputs[qr.quot].outputsCount) {
      lcdDrawChar(x+2, y+1, '1'+qr.quot, TINSIZE);
      lcdDrawFilledRect(x, y, 7, 7);
      lcdDrawSizedText(x+8, y, scriptInputsOutputs[qr.quot].outputs[qr.rem].name, att & STREXPANDED ? 9 : 4, att);
    }
    else
#endif
    {
      drawStringWithIndex(x, y, "LUA", qr.quot+1, att);
      lcdDrawChar(lcdLastPos, y, 'a'+qr.rem, att);
    }
  }

  else if (idx < MIXSRC_LAST_POT) {
    idx = idx-MIXSRC_Rud;
    if (ZEXIST(g_eeGeneral.anaNames[idx])) {
      if (idx < MIXSRC_FIRST_POT-MIXSRC_Rud )
        lcdDrawChar(x, y, '\307', att); //stick symbol
      else
        lcdDrawChar(x, y, '\311', att); //slider symbol
      lcdDrawSizedText(lcdNextPos, y, g_eeGeneral.anaNames[idx], LEN_ANA_NAME, ZCHAR|att);
    }
    else
      lcdDrawTextAtIndex(x, y, STR_VSRCRAW, idx+1, att);
  }
  else if (idx >= MIXSRC_FIRST_SWITCH && idx <= MIXSRC_LAST_SWITCH) {
    idx = idx-MIXSRC_FIRST_SWITCH;
    if (ZEXIST(g_eeGeneral.switchNames[idx])) {
      lcdDrawChar(x, y, '\312', att); //switch symbol
      lcdDrawSizedText(lcdNextPos, y, g_eeGeneral.switchNames[idx], LEN_SWITCH_NAME, ZCHAR|att);
    }
    else
      lcdDrawTextAtIndex(x, y, STR_VSRCRAW, idx+MIXSRC_FIRST_SWITCH-MIXSRC_Rud+1, att);
  }
  else if (idx < MIXSRC_SW1)
    lcdDrawTextAtIndex(x, y, STR_VSRCRAW, idx-MIXSRC_Rud+1, att);
  else if (idx <= MIXSRC_LAST_LOGICAL_SWITCH)
    putsSwitches(x, y, SWSRC_SW1+idx-MIXSRC_SW1, att);
  else if (idx < MIXSRC_CH1)
    drawStringWithIndex(x, y, STR_PPM_TRAINER, idx-MIXSRC_FIRST_TRAINER+1, att);
  else if (idx <= MIXSRC_LAST_CH) {
    drawStringWithIndex(x, y, STR_CH, idx-MIXSRC_CH1+1, att);
    if (ZEXIST(g_model.limitData[idx-MIXSRC_CH1].name) && (att & STREXPANDED)) {
      lcdDrawChar(lcdLastPos, y, ' ', att|SMLSIZE);
      lcdDrawSizedText(lcdLastPos+3, y, g_model.limitData[idx-MIXSRC_CH1].name, LEN_CHANNEL_NAME, ZCHAR|att|SMLSIZE);
    }
  }
  else if (idx <= MIXSRC_LAST_GVAR) {
    drawStringWithIndex(x, y, STR_GV, idx-MIXSRC_GVAR1+1, att);
  }
  else if (idx < MIXSRC_FIRST_TELEM) {
    lcdDrawTextAtIndex(x, y, STR_VSRCRAW, idx-MIXSRC_Rud+1-NUM_LOGICAL_SWITCH-NUM_TRAINER-NUM_CHNOUT-MAX_GVARS, att);
  }
  else {
    idx -= MIXSRC_FIRST_TELEM;
    div_t qr = div(idx, 3);
    lcdDrawSizedText(x, y, g_model.telemetrySensors[qr.quot].label, TELEM_LABEL_LEN, ZCHAR|att);
    if (qr.rem) lcdDrawChar(lcdLastPos, y, qr.rem==2 ? '+' : '-', att);
  }
}

void putsChnLetter(coord_t x, coord_t y, uint8_t idx, LcdFlags att)
{
  lcdDrawTextAtIndex(x, y, STR_RETA123, idx-1, att);
}

void putsModelName(coord_t x, coord_t y, char *name, uint8_t id, LcdFlags att)
{
  uint8_t len = sizeof(g_model.header.name);
  while (len>0 && !name[len-1]) --len;
  if (len==0) {
    drawStringWithIndex(x, y, STR_MODEL, id+1, att|LEADING0);
  }
  else {
    lcdDrawSizedText(x, y, name, sizeof(g_model.header.name), ZCHAR|att);
  }
}

void putsSwitches(coord_t x, coord_t y, int8_t idx, LcdFlags att)
{
  if (idx == SWSRC_OFF)
    return lcdDrawTextAtIndex(x, y, STR_OFFON, 0, att);
  if (idx < 0) {
    lcdDrawChar(x-2, y, '!', att);
    idx = -idx;
  }
#if defined(CPUARM) && defined(FLIGHT_MODES)
  if (idx >= SWSRC_FIRST_FLIGHT_MODE) {
    return drawStringWithIndex(x, y, STR_FP, idx-SWSRC_FIRST_FLIGHT_MODE, att);
  }
#endif
  return lcdDrawTextAtIndex(x, y, STR_VSWITCHES, idx, att);
}

#if defined(FLIGHT_MODES)
void putsFlightMode(coord_t x, coord_t y, int8_t idx, LcdFlags att)
{
  if (idx==0) { lcdDrawTextAtIndex(x, y, STR_MMMINV, 0, att); return; }
  if (idx < 0) { lcdDrawChar(x-2, y, '!', att); idx = -idx; }
  if (att & CONDENSED)
    lcdDrawNumber(x+FW*1, y, idx-1, (att & ~CONDENSED), 1);
  else
    drawStringWithIndex(x, y, STR_FP, idx-1, att);
}
#endif

#if defined(VIRTUALINPUTS)
void putsCurveRef(coord_t x, coord_t y, CurveRef &curve, LcdFlags att)
{
  if (curve.value != 0) {
    switch (curve.type) {
      case CURVE_REF_DIFF:
        lcdDrawChar(x, y, 'D', att);
        GVAR_MENU_ITEM(x+FW, y, curve.value, -100, 100, LEFT|att, 0, 0);
        break;

      case CURVE_REF_EXPO:
        lcdDrawChar(x, y, 'E', att);
        GVAR_MENU_ITEM(x+FW, y, curve.value, -100, 100, LEFT|att, 0, 0);
        break;

      case CURVE_REF_FUNC:
        lcdDrawTextAtIndex(x, y, STR_VCURVEFUNC, curve.value, att);
        break;

      case CURVE_REF_CUSTOM:
        drawCurveName(x, y, curve.value, att);
        break;
    }
  }
}
#endif

void drawCurveName(coord_t x, coord_t y, int8_t idx, LcdFlags att)
{
  if (idx < 0) {
    lcdDrawChar(x-3, y, '!', att);
    idx = -idx+CURVE_BASE-1;
  }
  if (idx < CURVE_BASE)
    lcdDrawTextAtIndex(x, y, STR_VCURVEFUNC, idx, att);
  else
    drawStringWithIndex(x, y, STR_CV, idx-CURVE_BASE+1, att);
}

void putsTimerMode(coord_t x, coord_t y, int8_t mode, LcdFlags att)
{
  if (mode >= 0) {
    if (mode < TMRMODE_COUNT)
      return lcdDrawTextAtIndex(x, y, STR_VTMRMODES, mode, att);
    else
      mode -= (TMRMODE_COUNT-1);
  }
  putsSwitches(x, y, mode, att);
}

void putsTrimMode(coord_t x, coord_t y, uint8_t phase, uint8_t idx, LcdFlags att)
{
  trim_t v = getRawTrimValue(phase, idx);

  if (v.value > TRIM_EXTENDED_MAX) {
    uint8_t p = v.value - TRIM_EXTENDED_MAX - 1;
    if (p >= phase) p++;
    lcdDrawChar(x, y, '0'+p, att);
  }
  else {
    putsChnLetter(x, y, idx+1, att);
  }
}

#if ROTARY_ENCODERS > 0
void putsRotaryEncoderMode(coord_t x, coord_t y, uint8_t phase, uint8_t idx, LcdFlags att)
{
  int16_t v = flightModeAddress(phase)->rotaryEncoders[idx];

  if (v > ROTARY_ENCODER_MAX) {
    uint8_t p = v - ROTARY_ENCODER_MAX - 1;
    if (p >= phase) p++;
    lcdDrawChar(x, y, '0'+p, att);
  }
  else {
    lcdDrawChar(x, y, 'a'+idx, att);
  }
}
#endif

#if defined(CPUARM)
const pm_uint8_t bchunit_ar[] PROGMEM = {
  UNIT_DIST,    // Alt
  UNIT_RAW,     // Rpm
  UNIT_PERCENT, // Fuel
  UNIT_TEMPERATURE, // T1
  UNIT_TEMPERATURE, // T2
  UNIT_KTS,     // Speed
  UNIT_DIST,    // Dist
  UNIT_DIST,    // GPS Alt
};

void putsValueWithUnit(coord_t x, coord_t y, lcdint_t val, uint8_t unit, LcdFlags att)
{
  // convertUnit(val, unit);
  lcdDrawNumber(x, y, val, att & (~NO_UNIT));
  if (!(att & NO_UNIT) && unit != UNIT_RAW) {
    lcdDrawTextAtIndex(lcdLastPos/*+1*/, y, STR_VTELEMUNIT, unit, 0);
  }
}

void displayGpsCoord(coord_t x, coord_t y, int32_t value, const char * direction, LcdFlags att, bool seconds=true)
{
  uint32_t absvalue = abs(value);
  lcdDrawNumber(x, y, absvalue / 1000000, att); // ddd
  lcdDrawChar(lcdLastPos, y, '@', att);
  absvalue = absvalue % 1000000;
  absvalue *= 60;
  if (g_eeGeneral.gpsFormat == 0 || !seconds) {
    lcdDrawNumber(lcdNextPos, y, absvalue / 1000000, att|LEFT|LEADING0, 2); // mm before '.'
    lcdDrawSolidVerticalLine(lcdLastPos, y, 2);
    lcdLastPos += 1;
    if (seconds) {
      absvalue %= 1000000;
      absvalue *= 60;
      absvalue /= 10000;
      lcdDrawNumber(lcdLastPos+2, y, absvalue, att|LEFT|PREC2);
      lcdDrawSolidVerticalLine(lcdLastPos, y, 2);
      lcdDrawSolidVerticalLine(lcdLastPos+2, y, 2);
      lcdLastPos += 3;
    }
  }
  else {
    absvalue /= 10000;
    lcdDrawNumber(lcdLastPos+FW, y, absvalue, att|LEFT|PREC2); // mm.mmm
  }
  lcdDrawSizedText(lcdLastPos+1, y, direction + (value>=0 ? 0 : 1), 1);
}

void displayDate(coord_t x, coord_t y, TelemetryItem & telemetryItem, LcdFlags att)
{
  if (att & DBLSIZE) {
    x -= 42;
    att &= ~0x0F00; // TODO constant
    lcdDrawNumber(x, y, telemetryItem.datetime.day, att|LEADING0|LEFT, 2);
    lcdDrawChar(lcdLastPos-1, y, '-', att);
    lcdDrawNumber(lcdNextPos-1, y, telemetryItem.datetime.month, att|LEFT, 2);
    lcdDrawChar(lcdLastPos-1, y, '-', att);
    lcdDrawNumber(lcdNextPos-1, y, telemetryItem.datetime.year, att|LEFT);
    y += FH;
    lcdDrawNumber(x, y, telemetryItem.datetime.hour, att|LEADING0|LEFT, 2);
    lcdDrawChar(lcdLastPos, y, ':', att);
    lcdDrawNumber(lcdNextPos, y, telemetryItem.datetime.min, att|LEADING0|LEFT, 2);
    lcdDrawChar(lcdLastPos, y, ':', att);
    lcdDrawNumber(lcdNextPos, y, telemetryItem.datetime.sec, att|LEADING0|LEFT, 2);
  }
  else {
    lcdDrawNumber(x, y, telemetryItem.datetime.hour, att|LEADING0|LEFT, 2);
    lcdDrawChar(lcdLastPos, y, ':', att);
    lcdDrawNumber(lcdNextPos, y, telemetryItem.datetime.min, att|LEADING0|LEFT, 2);
    lcdDrawChar(lcdLastPos, y, ':', att);
    lcdDrawNumber(lcdNextPos, y, telemetryItem.datetime.sec, att|LEADING0|LEFT, 2);
  }
}

void displayGpsCoords(coord_t x, coord_t y, TelemetryItem & telemetryItem, LcdFlags att)
{
  if (att & DBLSIZE) {
    x -= (g_eeGeneral.gpsFormat == 0 ? 54 : 51);
    att &= ~0x0F00; // TODO constant
    displayGpsCoord(x, y, telemetryItem.gps.longitude, "EW", att);
    displayGpsCoord(x, y+FH, telemetryItem.gps.latitude, "NS", att);
  }
  else {
    displayGpsCoord(x, y, telemetryItem.gps.longitude, "EW", att, false);
    displayGpsCoord(lcdNextPos+FWNUM, y, telemetryItem.gps.latitude, "NS", att, false);
  }
}

void putsTelemetryChannelValue(coord_t x, coord_t y, uint8_t channel, lcdint_t value, LcdFlags att)
{
  if (channel >= MAX_SENSORS) return;
  TelemetryItem & telemetryItem = telemetryItems[channel];
  TelemetrySensor & telemetrySensor = g_model.telemetrySensors[channel];
  if (telemetrySensor.unit == UNIT_DATETIME) {
    displayDate(x, y, telemetryItem, att);
  }
  else if (telemetrySensor.unit == UNIT_GPS) {
    displayGpsCoords(x, y, telemetryItem, att);
  }
  else {
    LcdFlags flags = att;
    if (telemetrySensor.prec==2)
      flags |= PREC2;
    else if (telemetrySensor.prec==1)
      flags |= PREC1;
    putsValueWithUnit(x, y, value, telemetrySensor.unit == UNIT_CELLS ? UNIT_VOLTS : telemetrySensor.unit, flags);
  }
}

void putsChannelValue(coord_t x, coord_t y, source_t channel, lcdint_t value, LcdFlags att)
{
  if (channel >= MIXSRC_FIRST_TELEM) {
    channel = (channel-MIXSRC_FIRST_TELEM) / 3;
    putsTelemetryChannelValue(x, y, channel, value, att);
  }
  else if (channel >= MIXSRC_FIRST_TIMER || channel == MIXSRC_TX_TIME) {
    putsTimer(x, y, value, att, att);
  }
  else if (channel == MIXSRC_TX_VOLTAGE) {
    lcdDrawNumber(x, y, value, att|PREC1);
  }
  else {
    if (channel <= MIXSRC_LAST_CH) {
      value = calcRESXto100(value);
    }
    lcdDrawNumber(x, y, value, att);
  }
}

void putsChannel(coord_t x, coord_t y, source_t channel, LcdFlags att)
{
  getvalue_t value = getValue(channel);
  putsChannelValue(x, y, channel, value, att);
}

#elif defined(FRSKY)
void putsValueWithUnit(coord_t x, coord_t y, lcdint_t val, uint8_t unit, LcdFlags att)
{
  convertUnit(val, unit);
  lcdDrawNumber(x, y, val, att & (~NO_UNIT));
  if (!(att & NO_UNIT) && unit != UNIT_RAW) {
    lcdDrawTextAtIndex(lcdLastPos/*+1*/, y, STR_VTELEMUNIT, unit, 0);
  }
}

const pm_uint8_t bchunit_ar[] PROGMEM = {
  UNIT_DIST,    // Alt
  UNIT_RAW,     // Rpm
  UNIT_PERCENT, // Fuel
  UNIT_TEMPERATURE, // T1
  UNIT_TEMPERATURE, // T2
  UNIT_KTS,     // Speed
  UNIT_DIST,    // Dist
  UNIT_DIST,    // GPS Alt
};

void putsTelemetryChannelValue(coord_t x, coord_t y, uint8_t channel, lcdint_t val, LcdFlags att)
{
  switch (channel) {
#if defined(CPUARM) && defined(RTCLOCK)
    case TELEM_TX_TIME-1:
    {
      putsRtcTime(x, y, att);
      break;
    }
#endif
    case TELEM_TIMER1-1:
    case TELEM_TIMER2-1:
#if defined(CPUARM)
    case TELEM_TIMER3-1:
#endif
      att &= ~NO_UNIT;
      putsTimer(x, y, val, att, att);
      break;
#if defined(FRSKY)
    case TELEM_MIN_A1-1:
    case TELEM_MIN_A2-1:
#if defined(CPUARM)
    case TELEM_MIN_A3-1:
    case TELEM_MIN_A4-1:
#endif
      channel -= TELEM_MIN_A1-TELEM_A1;
      // no break
    case TELEM_A1-1:
    case TELEM_A2-1:
#if defined(CPUARM)
    case TELEM_A3-1:
    case TELEM_A4-1:
#endif
      channel -= TELEM_A1-1;
      // A1 and A2
    {
      lcdint_t converted_value = applyChannelRatio(channel, val);
      if (ANA_CHANNEL_UNIT(channel) >= UNIT_RAW) {
        converted_value = div_and_round(converted_value, 10);
      }
      else {
        if (abs(converted_value) < 1000) {
          att |= PREC2;
        }
        else {
          converted_value = div_and_round(converted_value, 10);
          att |= PREC1;
        }
      }
      putsValueWithUnit(x, y, converted_value, g_model.frsky.channels[channel].type, att);
      break;
    }
#endif

    case TELEM_CELL-1:
    case TELEM_MIN_CELL-1:
      putsValueWithUnit(x, y, val, UNIT_VOLTS, att|PREC2);
      break;

    case TELEM_TX_VOLTAGE-1:
    case TELEM_VFAS-1:
    case TELEM_CELLS_SUM-1:
    case TELEM_MIN_CELLS_SUM-1:
    case TELEM_MIN_VFAS-1:
      putsValueWithUnit(x, y, val, UNIT_VOLTS, att|PREC1);
      break;

    case TELEM_CURRENT-1:
    case TELEM_MAX_CURRENT-1:
      putsValueWithUnit(x, y, val, UNIT_AMPS, att|PREC1);
      break;

    case TELEM_CONSUMPTION-1:
      putsValueWithUnit(x, y, val, UNIT_MAH, att);
      break;

    case TELEM_POWER-1:
    case TELEM_MAX_POWER-1:
      putsValueWithUnit(x, y, val, UNIT_WATTS, att);
      break;

    case TELEM_ACCx-1:
    case TELEM_ACCy-1:
    case TELEM_ACCz-1:
      putsValueWithUnit(x, y, val, UNIT_RAW, att|PREC2);
      break;

    case TELEM_VSPEED-1:
      putsValueWithUnit(x, y, div_and_round(val, 10), UNIT_RAW, att|PREC1);
      break;

    case TELEM_ASPEED-1:
    case TELEM_MAX_ASPEED-1:
      putsValueWithUnit(x, y, val, UNIT_KTS, att|PREC1);
      break;

#if defined(CPUARM)
    case TELEM_SWR-1:
#endif
    case TELEM_RSSI_TX-1:
    case TELEM_RSSI_RX-1:
      putsValueWithUnit(x, y, val, UNIT_RAW, att);
      break;

    case TELEM_HDG-1:
      putsValueWithUnit(x, y, val, UNIT_HDG, att);
      break;

#if defined(FRSKY_SPORT)
    case TELEM_ALT-1:
      putsValueWithUnit(x, y, div_and_round(val, 10), UNIT_DIST, att|PREC1);
      break;
#elif defined(WS_HOW_HIGH)
    case TELEM_ALT-1:
    case TELEM_MIN_ALT-1:
    case TELEM_MAX_ALT-1:
      if (IS_IMPERIAL_ENABLE() && IS_USR_PROTO_WS_HOW_HIGH()) {
        putsValueWithUnit(x, y, val, UNIT_FEET, att);
        break;
      }
      // no break
#endif

    default:
    {
      uint8_t unit = 1;
      if (channel >= TELEM_MAX_T1-1 && channel <= TELEM_MAX_DIST-1)
        channel -= TELEM_MAX_T1 - TELEM_T1;
      if (channel <= TELEM_GPSALT-1)
        unit = channel + 1 - TELEM_ALT;
      if (channel >= TELEM_MIN_ALT-1 && channel <= TELEM_MAX_ALT-1)
        unit = 0;
      putsValueWithUnit(x, y, val, pgm_read_byte(bchunit_ar+unit), att);
      break;
    }
  }
}
#else // defined(FRSKY)
void putsTelemetryChannelValue(coord_t x, coord_t y, uint8_t channel, lcdint_t val, uint8_t att)
{
  switch (channel) {
    case TELEM_TIMER1-1:
    case TELEM_TIMER2-1:
      att &= ~NO_UNIT;
      putsTimer(x, y, val, att, att);
      break;

    case TELEM_TX_VOLTAGE-1:
      lcdDrawNumber(x, y, val, (att|PREC1) & (~NO_UNIT));
      if (!(att & NO_UNIT))
        lcdDrawChar(lcdLastPos/*+1*/, y, 'V');
      break;
  }
}
#endif

void lcdSetContrast()
{
  lcdSetRefVolt(g_eeGeneral.contrast);
}

#define LCD_BYTE_FILTER(p, keep, add) *(p) = (*(p) & (keep)) | (add)

#if !defined(CPUARM)
void lcdDrawChar(coord_t x, uint8_t y, const unsigned char c, LcdFlags flags)
{
  uint8_t *p = &displayBuf[ y / 8 * LCD_W + x ];
  const pm_uchar *q = &font_5x7[(c-0x20)*5];

  lcdNextPos = x-1;
  p--;

  bool inv = false;
  if (flags & BLINK) {
    if (BLINK_ON_PHASE) {
      if (flags & INVERS)
        inv = true;
      else {
        return;
      }
    }
  }
  else if (flags & INVERS) {
    inv = true;
  }

  unsigned char c_remapped = 0;

#if defined(BOLD_SPECIFIC_FONT)
  if (flags & (DBLSIZE+BOLD)) {
#else
  if (flags & DBLSIZE) {
#endif
    // To save space only some DBLSIZE and BOLD chars are available
    // c has to be remapped. All non existing chars mapped to 0 (space)

    if (c>=',' && c<=':')
      c_remapped = c - ',' + 1;
    else if (c>='A' && c<='Z')
      c_remapped = c - 'A' + 16;
    else if (c>='a' && c<='z')
      c_remapped = c - 'a' + 42;
    else if (c=='_')
      c_remapped = 4;
#if defined(BOLD_SPECIFIC_FONT)
    else if (c!=' ')
      flags &= ~BOLD;
#endif

#if defined(BOLD_SPECIFIC_FONT)
  }
  if (flags & DBLSIZE) {
#endif

    /* each letter consists of ten top bytes followed by
     * by ten bottom bytes (20 bytes per * char) */
    q = &font_10x14[((uint16_t)c_remapped)*20];
    for (int8_t i=0; i<=11; i++) {
      uint8_t b1=0, b2=0;
      if (!i) {
        if (!x || !inv) {
          lcdNextPos++;
          p++;
          continue;
        }
      }
      else if (i <= 10) {
        b1 = pgm_read_byte(q++); /*top byte*/
        b2 = pgm_read_byte(q++);
      }
      if ((b1 & b2) == 0xff) continue;
      if (inv) {
        b1 = ~b1;
        b2 = ~b2;
      }
      if(p+LCD_W < DISPLAY_END) {
        ASSERT_IN_DISPLAY(p);
        ASSERT_IN_DISPLAY(p+LCD_W);
        LCD_BYTE_FILTER(p, 0, b1);
        LCD_BYTE_FILTER(p+LCD_W, 0, b2);
        p++;
        lcdNextPos++;
      }
    }
  }
  else {
    const uint8_t ym8 = (y & 0x07);
#if defined(BOLD_FONT)
  #if defined(BOLD_SPECIFIC_FONT)
    if (flags & BOLD) {
      q = &font_5x7_B[(c_remapped)*5];
    }
  #else
    uint8_t bb = 0;
    if (inv) bb = 0xff;
  #endif
#endif

    uint8_t *lineEnd = &displayBuf[ y / 8 * LCD_W + LCD_W ];

    for (int8_t i=0; i<=6; i++) {
      uint8_t b = 0;
      if (i==0) {
        if (!x || !inv) {
          lcdNextPos++;
          p++;
          continue;
        }
      }
      else if (i <= 5) {
        b = pgm_read_byte(q++);
      }
      if (b == 0xff) {
        if (flags & FIXEDWIDTH)
          b = 0;
        else
          continue;
      }
      if (inv) b = ~b;
      if ((flags & CONDENSED) && i==2) {
        /*condense the letter by skipping column 3 */
        continue;
      }

#if defined(BOLD_FONT) && !defined(BOLD_SPECIFIC_FONT)
      if (flags & BOLD) {
        uint8_t a;
        if (inv)
          a = b & bb;
        else
          a = b | bb;
        bb = b;
        b = a;
      }
#endif

      if (p<DISPLAY_END && p<lineEnd) {
        ASSERT_IN_DISPLAY(p);
        uint8_t mask = ~(0xff << ym8);
        LCD_BYTE_FILTER(p, mask, b << ym8);
        if (ym8) {
          uint8_t *r = p + LCD_W;
          if (r<DISPLAY_END)
            LCD_BYTE_FILTER(r, ~mask, b >> (8-ym8));
        }

        if (inv && (ym8 == 1)) *p |= 0x01;
      }
      p++;
      lcdNextPos++;
    }
  }
}
#endif

void lcdMaskPoint(uint8_t *p, uint8_t mask, LcdFlags att)
{
  ASSERT_IN_DISPLAY(p);

  if (att & FORCE)
    *p |= mask;
  else if (att & ERASE)
    *p &= ~mask;
  else
    *p ^= mask;
}

void lcdDrawPoint(coord_t x, coord_t y, LcdFlags att)
{
  uint8_t *p = &displayBuf[ y / 8 * LCD_W + x ];
  if (p<DISPLAY_END)
    lcdMaskPoint(p, BITMASK(y%8), att);
}

void lcdDrawHorizontalLine(coord_t x, coord_t y, coord_t w, uint8_t pat, LcdFlags att)
{
  if (y >= LCD_H) return;
  if (x+w > LCD_W) { w = LCD_W - x; }

  uint8_t *p  = &displayBuf[ y / 8 * LCD_W + x ];
  uint8_t msk = BITMASK(y%8);
  while (w--) {
    if(pat&1) {
      lcdMaskPoint(p, msk, att);
      pat = (pat >> 1) | 0x80;
    }
    else {
      pat = pat >> 1;
    }
    p++;
  }
}

#if defined(CPUM64)
void lcdDrawVerticalLine(coord_t x, int8_t y, int8_t h, uint8_t pat)
{
  if (x >= LCD_W) return;
  if (h<0) { y+=h; h=-h; }
  if (y<0) { h+=y; y=0; }
  if (y+h > LCD_H) { h = LCD_H - y; }

  if (pat==DOTTED && !(y%2))
    pat = ~pat;

  uint8_t *p  = &displayBuf[ y / 8 * LCD_W + x ];
  y = (y & 0x07);
  if (y) {
    ASSERT_IN_DISPLAY(p);
    *p ^= ~(BITMASK(y)-1) & pat;
    p += LCD_W;
    h -= 8-y;
  }
  while (h>0) {
    ASSERT_IN_DISPLAY(p);
    *p ^= pat;
    p += LCD_W;
    h -= 8;
  }
  if (h < 0) h += 8;
  if (h) {
    p -= LCD_W;
    ASSERT_IN_DISPLAY(p);
    *p ^= ~(BITMASK(h)-1) & pat;
  }
}
#else
// allows the att parameter...
void lcdDrawVerticalLine(coord_t x, scoord_t y, scoord_t h, uint8_t pat, LcdFlags att)
{
  if (x >= LCD_W) return;
#if defined(CPUARM)
  // should never happen on 9X
  if (y >= LCD_H) return;
#endif

  if (h<0) { y+=h; h=-h; }
  if (y<0) { h+=y; y=0; }
  if (y+h > LCD_H) { h = LCD_H - y; }

  if (pat==DOTTED && !(y%2))
    pat = ~pat;

  uint8_t *p  = &displayBuf[ y / 8 * LCD_W + x ];
  y = (y & 0x07);
  if (y) {
    ASSERT_IN_DISPLAY(p);
    uint8_t msk = ~(BITMASK(y)-1);
    h -= 8-y;
    if (h < 0)
      msk -= ~(BITMASK(8+h)-1);
    lcdMaskPoint(p, msk & pat, att);
    p += LCD_W;
  }
  while (h>=8) {
    ASSERT_IN_DISPLAY(p);
    lcdMaskPoint(p, pat, att);
    p += LCD_W;
    h -= 8;
  }
  if (h>0) {
    ASSERT_IN_DISPLAY(p);
    lcdMaskPoint(p, (BITMASK(h)-1) & pat, att);
  }
}
#endif

void lcdInvertLine(int8_t y)
{
  uint8_t *p  = &displayBuf[y * LCD_W];
  for (coord_t x=0; x<LCD_W; x++) {
    ASSERT_IN_DISPLAY(p);
    *p++ ^= 0xff;
  }
}

#define LCD_IMG_FUNCTION(NAME, TYPE, READ_BYTE)                               \
void NAME(coord_t x, coord_t y, TYPE img, uint8_t idx, LcdFlags att)          \
{                                                                             \
  TYPE q = img;                                                               \
  uint8_t w = READ_BYTE(q++);                                                 \
  uint8_t hb = (READ_BYTE(q++)+7)/8;                                          \
  bool inv = (att & INVERS) ? true : (att & BLINK ? BLINK_ON_PHASE : false);  \
  q += idx*w*hb;                                                              \
  for (uint8_t yb = 0; yb < hb; yb++) {                                       \
    uint8_t *p = &displayBuf[ (y / 8 + yb) * LCD_W + x ];                     \
    for (coord_t i=0; i<w; i++){                                              \
      uint8_t b = READ_BYTE(q);                                               \
      q++;                                                                    \
      ASSERT_IN_DISPLAY(p);                                                   \
      *p++ = inv ? ~b : b;                                                    \
    }                                                                         \
  }                                                                           \
}

#if defined(PCBMEGA2560) && !defined(SIMU)
LCD_IMG_FUNCTION(lcd_imgfar, uint_farptr_t, pgm_read_byte_far)
#endif

LCD_IMG_FUNCTION(lcd_img, const pm_uchar *, pgm_read_byte)

#endif
