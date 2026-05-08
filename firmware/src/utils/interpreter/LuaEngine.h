#pragma once
#include <Arduino.h>

extern "C" {
  #include "lua.h"
  #include "lauxlib.h"
  #include "lualib.h"
}

class LuaEngine {
public:
  // Unique address used as a sentinel to distinguish clean exit() from errors.
  static char exitSentinel;

  bool init();
  void deinit();

  // Hand the engine a script to run. Takes ownership of `src` (must be
  // allocated via heap_caps_malloc); the engine frees it after compile on
  // the Lua task. Returns false only if the engine is not ready.
  // Compile and execution both happen on the Lua task spawned by stepLoop().
  bool loadScript(char* src, size_t len, String& errOut);

  // Drive the script. First call spawns the Lua task (which compiles then
  // runs). Returns false when done (exit() called, error, or requestExit()).
  bool stepLoop(String& errOut);

  void requestExit()             { _exitRequested = true; }
  bool isExitRequested()   const { return _exitRequested; }

  // Body rect in screen coordinates — set once by the runner screen.
  void setBodyRect(int x, int y, int w, int h) {
    _bx = x; _by = y; _bw = w; _bh = h;
  }

private:
  enum Status : uint8_t {
    STATUS_IDLE,
    STATUS_RUNNING,
    STATUS_DONE_OK,
    STATUS_DONE_EXIT,
    STATUS_DONE_ERR,
  };

  lua_State* _lua      = nullptr;
  int        _chunkRef = LUA_NOREF;
  bool       _exitRequested = false;
  int        _bx = 0, _by = 0, _bw = 240, _bh = 200;
  uint32_t   _textFg = 0xFFFF;

  // Pending source: owned by the engine, freed by _taskEntry after compile.
  char*  _pendingSrc    = nullptr;
  size_t _pendingSrcLen = 0;

  void*             _task        = nullptr;  // TaskHandle_t (opaque to avoid header pulls)
  volatile Status   _status      = STATUS_IDLE;
  String            _taskErrOut;

  static void  _taskEntry(void* arg);
  static void* _alloc(void* ud, void* ptr, size_t osize, size_t nsize);
  static void  _countHook(lua_State* L, lua_Debug* ar);
  static LuaEngine* _fromState(lua_State* L);
  void _registerBindings();

  // uni.*
  static int _uni_debug(lua_State* L);
  static int _uni_delay(lua_State* L);
  static int _uni_heap(lua_State* L);
  static int _uni_millis(lua_State* L);
  static int _uni_beep(lua_State* L);
  static int _lua_exit(lua_State* L);

  // uni.lcd / uni.sd / uni.nav lazy loaders (registered in package.preload)
  static int _lua_load_lcd(lua_State* L);
  static int _lua_load_sd(lua_State* L);
  static int _lua_load_nav(lua_State* L);

  // uni.lcd.*
  static int _lcd_clear(lua_State* L);
  static int _lcd_fillScreen(lua_State* L);
  static int _lcd_print(lua_State* L);
  static int _lcd_rect(lua_State* L);
  static int _lcd_line(lua_State* L);
  static int _lcd_circle(lua_State* L);
  static int _lcd_fillCircle(lua_State* L);
  static int _lcd_roundRect(lua_State* L);
  static int _lcd_fillRoundRect(lua_State* L);
  static int _lcd_color(lua_State* L);
  static int _lcd_textSize(lua_State* L);
  static int _lcd_textColor(lua_State* L);
  static int _lcd_textDatum(lua_State* L);
  static int _lcd_textWidth(lua_State* L);
  static int _lcd_w(lua_State* L);
  static int _lcd_h(lua_State* L);

  // uni.lcd.sprite — userdata + methods via metatable
  static int _lcd_sprite(lua_State* L);
  static int _sprite_push(lua_State* L);
  static int _sprite_fill(lua_State* L);
  static int _sprite_rect(lua_State* L);
  static int _sprite_line(lua_State* L);
  static int _sprite_circle(lua_State* L);
  static int _sprite_fillCircle(lua_State* L);
  static int _sprite_roundRect(lua_State* L);
  static int _sprite_fillRoundRect(lua_State* L);
  static int _sprite_print(lua_State* L);
  static int _sprite_textColor(lua_State* L);
  static int _sprite_textSize(lua_State* L);
  static int _sprite_textDatum(lua_State* L);
  static int _sprite_textWidth(lua_State* L);
  static int _sprite_w(lua_State* L);
  static int _sprite_h(lua_State* L);
  static int _sprite_free(lua_State* L);
  static int _sprite_gc(lua_State* L);
  void _setupSpriteMetatable();

  // uni.sd.*
  static int _sd_read(lua_State* L);
  static int _sd_write(lua_State* L);
  static int _sd_append(lua_State* L);
  static int _sd_exists(lua_State* L);
  static int _sd_list(lua_State* L);
  static int _sd_remove(lua_State* L);
  static int _sd_rename(lua_State* L);
  static int _sd_mkdir(lua_State* L);
  static int _sd_size(lua_State* L);

  // uni.nav.*
  static int _nav_btn(lua_State* L);
  static int _nav_touchX(lua_State* L);
  static int _nav_touchY(lua_State* L);
  static int _nav_isTouched(lua_State* L);
};
