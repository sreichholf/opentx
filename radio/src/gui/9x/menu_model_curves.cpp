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

uint8_t s_curveChan;

int16_t curveFn(int16_t x)
{
  return applyCustomCurve(x, s_curveChan);
}

struct point_t {
  coord_t x;
  coord_t y;
};

point_t getPoint(uint8_t i)
{
  point_t result = {0, 0};
  CurveInfo & crv = g_model.curves[s_curveChan];
  int8_t * points = curveAddress(s_curveChan);
  bool custom = (crv.type == CURVE_TYPE_CUSTOM);
  uint8_t count = 5+crv.points;
  if (i < count) {
    result.x = X0-1-WCHART+i*WCHART*2/(count-1);
    result.y = (LCD_H-1) - (100 + points[i]) * (LCD_H-1) / 200;
    if (custom && i>0 && i<count-1)
      result.x = X0-1-WCHART + (100 + (100 + points[count+i-1]) * (2*WCHART)) / 200;
  }
  return result;
}

void drawFunction(FnFuncP fn, uint8_t offset)
{
  lcdDrawVerticalLine(X0-offset, 0, LCD_H, 0xee);
  lcdDrawHorizontalLine(X0-WCHART-offset, Y0, WCHART*2, 0xee);

  coord_t prev_yv = (coord_t)-1;

  for (int8_t xv=-WCHART; xv<=WCHART; xv++) {
    coord_t yv = (LCD_H-1) - (((uint16_t)RESX + fn(xv * (RESX/WCHART))) / 2 * (LCD_H-1) / RESX);
    if (prev_yv != (coord_t)-1) {
      if (abs((int8_t)yv-prev_yv) <= 1) {
        lcdDrawPoint(X0+xv-offset-1, prev_yv, FORCE);
      }
      else {
        uint8_t tmp = (prev_yv < yv ? 0 : 1);
        lcdDrawSolidVerticalLine(X0+xv-offset-1, yv+tmp, prev_yv-yv);
      }
    }
    prev_yv = yv;
  }
}

void DrawCurve(uint8_t offset=0)
{
  drawFunction(curveFn, offset);

  uint8_t i = 0;
  do {
    point_t point = getPoint(i);
    i++;
    if (point.x == 0) break;
    lcdDrawFilledRect(point.x-offset, point.y-1, 3, 3, SOLID, FORCE); // do markup square
  } while(1);
}

extern int8_t * curveEnd[MAX_CURVES];
bool moveCurve(uint8_t index, int8_t shift)
{
  if (curveEnd[MAX_CURVES-1] + shift > g_model.points + sizeof(g_model.points)) {
    AUDIO_WARNING2();
    return false;
  }

  int8_t *nextCrv = curveAddress(index+1);
  memmove(nextCrv+shift, nextCrv, 5*(MAX_CURVES-index-1)+curveEnd[MAX_CURVES-1]-curveEnd[index]);
  if (shift < 0) memclear(&g_model.points[NUM_POINTS-1] + shift, -shift);
  while (index<MAX_CURVES) {
    curveEnd[index++] += shift;
  }

  storageDirty(EE_MODEL);
  return true;
}

int8_t getCurveX(int noPoints, int point)
{
  return -100 + div_and_round((point*2000) / (noPoints-1), 10);
}

void resetCustomCurveX(int8_t * points, int noPoints)
{
  for (int i=0; i<noPoints-2; i++) {
    points[noPoints+i] = getCurveX(noPoints, i+1);
  }
}

void displayPresetChoice(uint8_t event)
{
  displayWarning(event);
  lcdDrawNumber(WARNING_LINE_X+FW*7, WARNING_LINE_Y, 45*warningInputValue/4, LEFT|INVERS);
  lcdDrawChar(lcdLastPos, WARNING_LINE_Y, '@', INVERS);

  if (warningResult) {
    warningResult = 0;
    CurveInfo & crv = g_model.curves[s_curveChan];
    int8_t * points = curveAddress(s_curveChan);
    int k = 25 * warningInputValue;
    int dx = 2000 / (5+crv.points-1);
    for (uint8_t i=0; i<5+crv.points; i++) {
      int x = -1000 + i * dx;
      points[i] = div_and_round(div_and_round(k * x, 100), 10);
    }
    if (crv.type == CURVE_TYPE_CUSTOM) {
      resetCustomCurveX(points, 5+crv.points);
    }
  }
}

void onCurveOneMenu(const char * result)
{
  if (result == STR_CURVE_PRESET) {
    POPUP_INPUT(STR_PRESET, displayPresetChoice, 0, -4, 4);
  }
  else if (result == STR_MIRROR) {
    CurveInfo & crv = g_model.curves[s_curveChan];
    int8_t * points = curveAddress(s_curveChan);
    for (int i=0; i<5+crv.points; i++)
      points[i] = -points[i];
  }
  else if (result == STR_CLEAR) {
    CurveInfo & crv = g_model.curves[s_curveChan];
    int8_t * points = curveAddress(s_curveChan);
    for (int i=0; i<5+crv.points; i++)
      points[i] = 0;
    if (crv.type == CURVE_TYPE_CUSTOM) {
      resetCustomCurveX(points, 5+crv.points);
    }
  }
}


void menuModelCurveOne(uint8_t event)
{
  static uint8_t pointsOfs = 0;
  CurveData & crv = g_model.curves[s_curveChan];
  int8_t * points = curveAddress(s_curveChan);

  lcdDrawText(11*FW+FW/2, 0, TR_PT "\002X\006Y");
  drawStringWithIndex(PSIZE(TR_MENUCURVES)*FW+FW, 0, "CV", s_curveChan+1);
  lcdDrawFilledRect(0, 0, LCD_W, FH, SOLID);

  SIMPLE_SUBMENU(STR_MENUCURVES, 4 + 5+crv.points + (crv.type==CURVE_TYPE_CUSTOM ? 5+crv.points-2 : 0));

  lcd_putsLeft(FH+1, STR_NAME);
  editName(INDENT_WIDTH, 2*FH+1, crv.name, sizeof(crv.name), event, menuVerticalPosition==0);

  uint8_t attr = (menuVerticalPosition==1 ? (s_editMode>0 ? INVERS|BLINK : INVERS) : 0);
  lcd_putsLeft(3*FH+1, STR_TYPE);
  lcdDrawTextAtIndex(INDENT_WIDTH, 4*FH+1, STR_CURVE_TYPES, crv.type, attr);
  if (attr) {
    uint8_t newType = checkIncDecModelZero(event, crv.type, CURVE_TYPE_LAST);
    if (newType != crv.type) {
      for (int i=1; i<4+crv.points; i++) {
        points[i] = calcRESXto100(applyCustomCurve(calc100toRESX(getCurveX(5+crv.points, i)), s_curveChan));
      }
      moveCurve(s_curveChan, checkIncDec_Ret > 0 ? 3+crv.points : -3-crv.points);
      if (newType == CURVE_TYPE_CUSTOM) {
        resetCustomCurveX(points, 5+crv.points);
      }
      crv.type = newType;
    }
  }

  attr = (menuVerticalPosition==2 ? (s_editMode>0 ? INVERS|BLINK : INVERS) : 0);
  lcd_putsLeft(5*FH+1, STR_COUNT);
  lcdDrawNumber(INDENT_WIDTH, 6*FH+1, 5+crv.points, LEFT|attr);
  lcdDrawText(lcdLastPos, 6*FH+1, STR_PTS, attr);
  if (attr) {
    int8_t count = checkIncDecModel(event, crv.points, -3, 12); // 2pts - 17pts
    if (checkIncDec_Ret) {
      int8_t newPoints[MAX_POINTS];
      newPoints[0] = points[0];
      newPoints[4+count] = points[4+crv.points];
      for (int i=1; i<4+count; i++)
        newPoints[i] = calcRESXto100(applyCustomCurve(calc100toRESX(getCurveX(5+count, i)), s_curveChan));
      moveCurve(s_curveChan, checkIncDec_Ret*(crv.type==CURVE_TYPE_CUSTOM?2:1));
      for (int i=0; i<5+count; i++) {
        points[i] = newPoints[i];
        if (crv.type == CURVE_TYPE_CUSTOM && i!=0 && i!=4+count)
          points[5+count+i-1] = getCurveX(5+count, i);
      }
      crv.points = count;
    }
  }

  lcd_putsLeft(7*FH+1, STR_SMOOTH);
  drawCheckBox(7 * FW, 7 * FH + 1, crv.smooth, menuVerticalPosition == 3 ? INVERS : 0);
  if (menuVerticalPosition==3) crv.smooth = checkIncDecModel(event, crv.smooth, 0, 1);

  switch(event) {
    case EVT_ENTRY:
      pointsOfs = 0;
      SET_SCROLLBAR_X(0);
      break;
    case EVT_KEY_LONG(KEY_ENTER):
      if (menuVerticalPosition > 1) {
        killEvents(event);
        POPUP_MENU_ADD_ITEM(STR_CURVE_PRESET);
        POPUP_MENU_ADD_ITEM(STR_MIRROR);
        POPUP_MENU_ADD_ITEM(STR_CLEAR);
        POPUP_MENU_START(onCurveOneMenu);
      }
      break;
  }

  DrawCurve(FW);

  uint8_t posY = FH+1;
  attr = (s_editMode > 0 ? INVERS|BLINK : INVERS);
  for (uint8_t i=0; i<5+crv.points; i++) {
    point_t point = getPoint(i);
    uint8_t selectionMode = 0;
    if (crv.type==CURVE_TYPE_CUSTOM) {
      if (menuVerticalPosition==4+2*i || (i==5+crv.points-1 && menuVerticalPosition==4+5+crv.points+5+crv.points-2-1))
        selectionMode = 2;
      else if (i>0 && menuVerticalPosition==3+2*i)
        selectionMode = 1;
    }
    else if (menuVerticalPosition == 4+i) {
      selectionMode = 2;
    }

    if (i>=pointsOfs && i<pointsOfs+7) {
      int8_t x = getCurveX(5+crv.points, i);
      if (crv.type==CURVE_TYPE_CUSTOM && i>0 && i<5+crv.points-1) x = points[5+crv.points+i-1];
      lcdDrawNumber(6+10*FW+FW/2, posY, i+1, LEFT);
      lcdDrawNumber(3+14*FW, posY, x, LEFT|(selectionMode==1?attr:0));
      lcdDrawNumber(3+18*FW, posY, points[i], LEFT|(selectionMode==2?attr:0));
      posY += FH;
    }

    if (selectionMode > 0) {
      // do selection square
      lcdDrawFilledRect(point.x-FW-1, point.y-2, 5, 5, SOLID, FORCE);
      lcdDrawFilledRect(point.x-FW, point.y-1, 3, 3, SOLID);
      if (s_editMode > 0) {
        if (selectionMode == 1)
          CHECK_INCDEC_MODELVAR(event, points[5+crv.points+i-1], i==1 ? -100 : points[5+crv.points+i-2], i==5+crv.points-2 ? 100 : points[5+crv.points+i]);  // edit X
        else if (selectionMode == 2)
          CHECK_INCDEC_MODELVAR(event, points[i], -100, 100);
      }
      if (i < pointsOfs)
        pointsOfs = i;
      else if (i > pointsOfs+6)
        pointsOfs = i-6;
    }
  }
}

void editCurveRef(coord_t x, coord_t y, CurveRef & curve, uint8_t event, uint8_t attr)
{
  lcdDrawTextAtIndex(x, y, "\004DiffExpoFuncCstm", curve.type, menuHorizontalPosition==0 ? attr : 0);
  if (attr && menuHorizontalPosition==0) {
    CHECK_INCDEC_MODELVAR_ZERO(event, curve.type, CURVE_REF_CUSTOM);
    if (checkIncDec_Ret) curve.value = 0;
  }
  switch (curve.type) {
    case CURVE_REF_DIFF:
    case CURVE_REF_EXPO:
      curve.value = GVAR_MENU_ITEM(x+5*FW+2, y, curve.value, -100, 100, menuHorizontalPosition==1 ? LEFT|attr : LEFT, 0, event);
      break;
    case CURVE_REF_FUNC:
      lcdDrawTextAtIndex(x+5*FW+2, y, STR_VCURVEFUNC, curve.value, menuHorizontalPosition==1 ? attr : 0);
      if (attr && menuHorizontalPosition==1) CHECK_INCDEC_MODELVAR_ZERO(event, curve.value, CURVE_BASE-1);
      break;
    case CURVE_REF_CUSTOM:
      drawCurveName(x+5*FW+2, y, curve.value, menuHorizontalPosition==1 ? attr : 0);
      if (attr && menuHorizontalPosition==1) {
        if (event==EVT_KEY_LONG(KEY_ENTER) && curve.value!=0) {
          s_curveChan = (curve.value<0 ? -curve.value-1 : curve.value-1);
          pushMenu(menuModelCurveOne);
        }
        else {
          CHECK_INCDEC_MODELVAR(event, curve.value, -MAX_CURVES, MAX_CURVES);
        }
      }
      break;
  }
}

void menuModelCurvesAll(uint8_t event)
{
  SIMPLE_MENU(STR_MENUCURVES, menuTabModel, e_CurvesAll, MAX_CURVES);

  int  sub = menuVerticalPosition;

  switch (event) {
    case EVT_KEY_BREAK(KEY_ENTER):
      if (!READ_ONLY()) {
        s_curveChan = sub;
        pushMenu(menuModelCurveOne);
      }
      break;
  }

  for (int i=0; i<LCD_LINES-1; i++) {
    coord_t y = MENU_HEADER_HEIGHT + 1 + i*FH;
    int k = i + menuVerticalOffset;
    LcdFlags attr = (sub == k ? INVERS : 0);
    {
      drawStringWithIndex(0, y, STR_CV, k+1, attr);
      CurveData & crv = g_model.curves[k];
      editName(4*FW, y, crv.name, sizeof(crv.name), 0, 0);
      lcdDrawNumber(11*FW, y, 5+crv.points, LEFT);
      lcdDrawText(lcdLastPos, y, STR_PTS, 0);
    }
  }

  s_curveChan = sub;
  DrawCurve(23);
}
