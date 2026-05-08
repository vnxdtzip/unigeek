-- dino.lua — Dino Jump game with sprite-buffered frames (no flicker).
-- Compose each frame into an off-screen sprite and push in one shot. Falls
-- back to direct lcd rendering if the sprite buffer can't be allocated.

local lcd = require("uni.lcd")
local sd  = require("uni.sd")
local nav = require("uni.nav")

local W, H = lcd.w(), lcd.h()
local GY         = H - 18
local DX, DW, DH = 22, 13, 17
local OW         = 11
local GRAVITY    = 0.75
local JUMP_FORCE = -9.5

local C_WHITE  = lcd.color(255, 255, 255)
local C_GREEN  = lcd.color( 60, 210,  80)
local C_DARK   = lcd.color(  0, 100,  30)
local C_ORANGE = lcd.color(255, 140,   0)
local C_RED    = lcd.color(255,  60,  60)
local C_YELLOW = lcd.color(255, 220,   0)
local C_GREY   = lcd.color( 70,  70,  70)
local C_BLACK  = lcd.color(  0,   0,   0)

-- Pick a draw target: sprite if it fits, else a thin shim around lcd.
-- Using method-call syntax (`t:rect(...)`) means the helpers below work the
-- same regardless of which one we got.
local sp = lcd.sprite(W, H)
local t

if sp then
  t = sp
else
  uni.debug("dino: sprite OOM, falling back to direct lcd")
  t = {
    fill       = function(_, c)             lcd.fillScreen(c) end,
    rect       = function(_, x,y,w,h,c)     lcd.rect(x,y,w,h,c) end,
    print      = function(_, x,y,s)         lcd.print(x,y,s) end,
    textSize   = function(_, n)             lcd.textSize(n) end,
    textColor  = function(_, fg, bg)
      if bg ~= nil then lcd.textColor(fg, bg) else lcd.textColor(fg) end
    end,
    push       = function() end,
  }
end

local hiScore = 0
local raw = sd.read("/unigeek/games/lua_dino.txt")
if raw and #raw > 0 then hiScore = math.floor(tonumber(raw) or 0) end

math.randomseed(math.floor(uni.millis()))

-- game state
local gstate = "idle"
local score, dinoY, jumpV, onGnd
local obsX, obsH, speed, tick, leg, newHi

local function _reset()
  score  = 0
  dinoY  = GY - DH
  jumpV  = 0
  onGnd  = true
  obsX   = W + 40
  obsH   = 18 + math.random(0, 18)
  speed  = 2.5
  tick   = 0
  newHi  = false
  leg    = 0
end

local function _drawDino(target, dy, running)
  local iy = math.floor(dy)
  target:rect(DX,          iy,     DW, DH, C_GREEN)
  target:rect(DX + DW - 4, iy - 4,  7,  6, C_GREEN)
  target:rect(DX + DW + 1, iy - 3,  2,  2, C_BLACK)
  target:rect(DX + DW - 4, iy + 2,  4,  2, C_DARK)
  if not running or math.floor(leg / 4) % 2 == 0 then
    target:rect(DX + 2, iy + DH,     4, 4, C_GREEN)
    target:rect(DX + 8, iy + DH + 1, 4, 3, C_GREEN)
  else
    target:rect(DX + 2, iy + DH + 1, 4, 3, C_GREEN)
    target:rect(DX + 8, iy + DH,     4, 4, C_GREEN)
  end
end

local function _drawCactus(target, ox, oh)
  local ix = math.floor(ox)
  local iy = math.floor(oh)
  target:rect(ix,       GY - iy, OW, iy, C_ORANGE)
  if iy > 14 then
    target:rect(ix - 4, GY - iy + 5, 4, 5, C_ORANGE)
    target:rect(ix - 4, GY - iy + 5, 5, 3, C_ORANGE)
  end
  if iy > 10 then
    target:rect(ix + OW,     GY - iy + 8, 4, 4, C_ORANGE)
    target:rect(ix + OW - 1, GY - iy + 8, 5, 3, C_ORANGE)
  end
end

local function _drawGround(target)
  target:rect(0, GY + 1, W, 2, C_GREY)
end

local function _drawIdle(target)
  target:fill(C_BLACK)
  _drawGround(target)
  target:textSize(2); target:textColor(C_WHITE)
  target:print(math.floor(W/2) - 52, math.floor(H/2) - 28, "DINO JUMP")
  target:textSize(1); target:textColor(C_GREY)
  target:print(math.floor(W/2) - 30, math.floor(H/2) - 4, "OK / UP to start")
  if hiScore > 0 then
    target:textColor(C_YELLOW)
    target:print(math.floor(W/2) - 28, math.floor(H/2) + 10, "Best: " .. hiScore)
  end
  _drawDino(target, GY - DH, false)
end

local function _drawOver(target)
  target:fill(C_BLACK)
  _drawGround(target)
  target:textSize(2); target:textColor(C_RED)
  target:print(math.floor(W/2) - 52, math.floor(H/2) - 26, "GAME OVER")
  target:textSize(1); target:textColor(C_WHITE)
  target:print(math.floor(W/2) - 26, math.floor(H/2) - 4, "Score: " .. score)
  if newHi then
    target:textColor(C_YELLOW)
    target:print(math.floor(W/2) - 28, math.floor(H/2) + 8, "NEW BEST!")
  else
    target:textColor(C_GREY)
    target:print(math.floor(W/2) - 28, math.floor(H/2) + 8, "Best:  " .. hiScore)
  end
  target:textColor(C_GREY)
  target:print(math.floor(W/2) - 32, math.floor(H/2) + 22, "OK: retry")
  _drawDino(target, GY - DH, false)
end

local function _drawPlay(target)
  target:fill(C_BLACK)
  _drawGround(target)
  _drawCactus(target, obsX, obsH)
  _drawDino(target, dinoY, onGnd)

  -- HUD: composed into the sprite alongside everything else, so it appears
  -- at the same instant as the dino+cactus — no torn-frame look.
  target:textSize(1)
  target:textColor(C_WHITE, C_BLACK)
  target:print(0, 0, string.format("Score:%-5d", score))
  if hiScore > 0 then
    target:textColor(C_GREY, C_BLACK)
    target:print(W - 54, 0, string.format("Best:%-4d", hiScore))
  end
end

local function _collision()
  local dy  = math.floor(dinoY)
  local ox  = math.floor(obsX)
  local oh  = math.floor(obsH)
  local TOL = 3
  return (DX + DW - TOL > ox + TOL) and
         (DX + TOL < ox + OW - TOL) and
         (dy + DH - TOL > GY - oh + TOL)
end

-- ── Main loop ─────────────────────────────────────────────────────────

local prevState = ""

while true do
  local btn     = nav.btn()
  if btn == "back" then break end
  local entered = (gstate ~= prevState)
  prevState = gstate

  if gstate == "idle" then
    if entered then
      _drawIdle(t)
      t:push(0, 0)
    end
    if btn == "ok" or btn == "up" then
      _reset()
      gstate = "play"
    end

  elseif gstate == "over" then
    if entered then
      _drawOver(t)
      t:push(0, 0)
    end
    if btn == "ok" or btn == "up" then
      _reset()
      gstate = "play"
    end

  else  -- play
    tick = tick + 1
    leg  = leg + 1

    if (btn == "ok" or btn == "up") and onGnd then
      jumpV = JUMP_FORCE
      onGnd = false
      uni.beep(1200, 20)
    end

    dinoY = dinoY + jumpV
    jumpV = jumpV + GRAVITY
    if dinoY >= GY - DH then
      dinoY = GY - DH
      jumpV = 0
      onGnd = true
    end

    obsX = obsX - speed
    if obsX < -OW - 10 then
      obsX  = W + math.random(30, 70)
      obsH  = 16 + math.random(0, 22)
      speed = speed + 0.04
    end

    if tick % 8 == 0 then score = score + 1 end
    if score > 0 and score % 50 == 0 and tick % 8 == 0 then
      uni.beep(660, 30)
    end

    _drawPlay(t)
    t:push(0, 0)

    if _collision() then
      uni.beep(150, 120)
      if score > hiScore then
        hiScore = score
        newHi   = true
        sd.write("/unigeek/games/lua_dino.txt", tostring(hiScore))
        uni.debug("new high score: " .. hiScore)
      end
      gstate = "over"
    end
  end

  uni.delay(16)
end

if sp then sp:free() end
