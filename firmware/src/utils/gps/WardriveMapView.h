//
// WardriveMapView — embeddable map view for wardrive CSV paths.
//
// Lives inside a parent screen (WigleScreen, GPSScreen, ...). The parent
// owns its own state machine and forwards nav/render to this view when in
// its map state. No separate screen is pushed.
//
// Tile source: ESRI World Imagery (free JPG), cached on storage at
// /unigeek/maps/{z}/{x}/{y}.jpg.
//

#pragma once

#include <Arduino.h>
#include "core/IStorage.h"
#include "core/INavigation.h"

class WardriveMapView
{
public:
  enum NavResult {
    NAV_IGNORED,    // direction wasn't handled (e.g. DIR_BACK — parent decides)
    NAV_HANDLED,    // view consumed the direction; parent should re-render
    NAV_HUD_ONLY,   // view already redrew its own overlay inline (axis toggle);
                    // parent must NOT trigger a full tile/path re-render
  };

  // Load path from CSV, set view center to bbox midpoint. Shows a status
  // popup while loading. Returns true if any valid points were loaded.
  bool init(IStorage* storage, const String& fileName);

  // Clear loaded state (call when leaving the map view).
  void reset();

  // Handle a nav direction. Returns NAV_IGNORED for DIR_BACK so the parent
  // can decide how to back out (e.g. to the file picker).
  NavResult onNav(INavigation::Direction dir);

  // Draw the map (tiles + path overlay + axis HUD) inside the given body
  // rect. Caches body dimensions so subsequent onNav() calls can compute
  // pan steps and tile bounds correctly.
  void render(int16_t bx, int16_t by, int16_t bw, int16_t bh);

private:
  static constexpr int      ZOOM            = 11;
  static constexpr uint16_t MAX_PATH_POINTS = 1024;
  static constexpr uint16_t TILE_SIZE       = 256;
  static constexpr const char* TILE_CACHE_DIR = "/unigeek/maps";

  enum Axis {
    AXIS_NS = 0,   // up=north, down=south
    AXIS_EW,       // up=west,  down=east
    AXIS_COUNT,
  };

  IStorage* _storage    = nullptr;
  bool      _pathLoaded = false;
  uint16_t  _pathCount  = 0;
  Axis      _axis       = AXIS_NS;

  // Path stored as lat/lng for zoom independence.
  float  _pathLat[MAX_PATH_POINTS];
  float  _pathLng[MAX_PATH_POINTS];

  // View center as lat/lng.
  double _centerLat = 0.0;
  double _centerLng = 0.0;

  // Cached body rect (set by render(), used by onNav and tile callback).
  int16_t _bx = 0, _by = 0, _bw = 0, _bh = 0;

  static int32_t _lng2px(double lng);
  static int32_t _lat2py(double lat);
  static bool    _parseLatLng(const char* line, size_t len, double& lat, double& lng);

  void    _loadPath(const String& fullPath);
  void    _recenterOnPath();
  int32_t _panStep() const;

  void _drawTiles();
  void _drawPath();
  void _drawAxisHud();

  static bool _ensureTile(IStorage* storage, int tx, int ty, String& outPath);
  static bool _fetchTile(int tx, int ty, IStorage* storage, const String& outPath);

  static bool _tileCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
  void _drawClippedLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
};
