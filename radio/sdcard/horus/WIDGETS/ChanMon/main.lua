local options = {
  { "Option1", VALUE, 32 }
}

local function create(zone, options)
  local myZone = { zone=zone, options=options}
  return myZone
end

local function update(myZone, options)
  myZone.options = options
end

local function drawChannels(x, y, w, h, firstchan, lastchan)
  for i=firstchan, lastchan, 1 do
    local chanVal = getValue('ch'..i) / 10.24
    lcd.drawText(x, y + (i - firstchan) * 20, "CH"..i)
    local offset = 78
    if lastchan > 9 then offset = 88 end
    lcd.drawText(x + offset, y + (i -firstchan) * 20, string.format("%.d", chanVal), RIGHT)
    lcd.drawRectangle(x + offset + 2, y + (i -firstchan) * 20, w - 90, 18)
    lcd.drawLine(x + offset + (w - 90) / 2 + 2,  y + (i -firstchan) * 20, x + offset + (w - 90) / 2 + 2,  18 + y + (i -firstchan) * 20, SOLID, 0)
    if chanVal > 0 then
      lcd.drawFilledRectangle(x + offset + 2 + (w - 90) / 2,  y + (i -firstchan) * 20, (w - 90) * chanVal / 200 , 18)
    elseif chanVal < 0 then
      local endpoint = x + offset + (w - 90) / 2 + 2
      local startpoint = x + offset + 2
      local size = math.floor(math.abs((endpoint - startpoint) * chanVal / 100))
      lcd.drawFilledRectangle(endpoint - size,  y + (i - firstchan) * 20, size, 18)
    end
  end
end

--- Size is 180x70
local function zoneMedium(zone)
  drawChannels(zone.zone.x, zone.zone.y, zone.zone.w, zone.zone.h, 1, 4)
end

--- Size is 190x150
local function zoneLarge(zone)
  drawChannels(zone.zone.x, zone.zone.y, zone.zone.w, zone.zone.h, 1, 8)
end

--- Size is 390x170
local function zoneXLarge(zone)
  lcd.drawLine(zone.zone.x + math.floor(zone.zone.w / 2), zone.zone.y, zone.zone.x + math.floor(zone.zone.w / 2), zone.zone.y + zone.zone.h, SOLID, 0)
  drawChannels(zone.zone.x, zone.zone.y, math.floor(zone.zone.w / 2), zone.zone.h, 1, 8)
  drawChannels(zone.zone.x + math.floor(zone.zone.w / 2) + 5, zone.zone.y, math.floor(zone.zone.w / 2), zone.zone.h, 9, 16)
end

function refresh(myZone)
  if myZone.zone.w  > 380 and myZone.zone.h > 165 then zoneXLarge(myZone)
  elseif myZone.zone.w  > 180 and myZone.zone.h > 145  then zoneLarge(myZone)
  elseif myZone.zone.w  > 170 and myZone.zone.h > 65 then zoneMedium(myZone)
  end
end

return { name="ChanMon", options=options, create=create, update=update, refresh=refresh }
