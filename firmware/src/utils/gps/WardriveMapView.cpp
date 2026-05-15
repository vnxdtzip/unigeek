#include "utils/gps/WardriveMapView.h"

#include <math.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>

#include "core/Device.h"
#include "utils/gps/WigleUtil.h"
#include "ui/actions/ShowStatusAction.h"

// ── Tile source ──────────────────────────────────────────
// ESRI World Imagery (free, no key). URL order is z/y/x.
static constexpr const char* TILE_URL_PREFIX =
  "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/";

// TJpg_Decoder uses a C-style callback; route via this pointer.
static WardriveMapView* g_mapInstance = nullptr;

// ── Web Mercator (fixed zoom 11) ─────────────────────────

int32_t WardriveMapView::_lng2px(double lng) {
  double worldSize = (double)(256 << ZOOM);
  return (int32_t)((lng + 180.0) / 360.0 * worldSize);
}

int32_t WardriveMapView::_lat2py(double lat) {
  double worldSize = (double)(256 << ZOOM);
  double latRad = lat * M_PI / 180.0;
  if (latRad >  1.4844) latRad =  1.4844;
  if (latRad < -1.4844) latRad = -1.4844;
  double mercN = log(tan(M_PI / 4.0 + latRad / 2.0));
  return (int32_t)(worldSize / 2.0 - worldSize * mercN / (2.0 * M_PI));
}

// ── CSV parsing ──────────────────────────────────────────

bool WardriveMapView::_parseLatLng(const char* line, size_t len, double& lat, double& lng) {
  uint8_t field   = 0;
  bool    inQuote = false;
  size_t  start   = 0;
  bool    haveLat = false, haveLng = false;

  for (size_t i = 0; i <= len; i++) {
    char c = (i < len) ? line[i] : ',';
    if (c == '"') { inQuote = !inQuote; continue; }
    if (c == ',' && !inQuote) {
      if (field == 7) { lat = atof(line + start); haveLat = true; }
      else if (field == 8) { lng = atof(line + start); haveLng = true; }
      field++;
      start = i + 1;
      if (haveLat && haveLng) break;
    }
  }
  if (!haveLat || !haveLng) return false;
  if (lat == 0.0 && lng == 0.0) return false;
  if (lat < -85.0 || lat > 85.0 || lng < -180.0 || lng > 180.0) return false;
  return true;
}

void WardriveMapView::_loadPath(const String& fullPath) {
  fs::File f = _storage->open(fullPath.c_str(), FILE_READ);
  if (!f) return;

  double sumLat = 0.0, sumLng = 0.0;
  uint32_t total = 0;
  char     line[256];
  uint8_t  headerSkip = 2;

  while (f.available()) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n')) line[--n] = '\0';
    if (n == 0) continue;
    if (headerSkip > 0) { headerSkip--; continue; }
    double lat, lng;
    if (!_parseLatLng(line, n, lat, lng)) continue;
    sumLat += lat;
    sumLng += lng;
    total++;
  }
  f.close();
  if (total == 0) return;

  uint16_t step = 1;
  while (total / step > MAX_PATH_POINTS) step++;

  f = _storage->open(fullPath.c_str(), FILE_READ);
  if (!f) return;
  headerSkip = 2;
  uint32_t idx = 0;
  _pathCount = 0;

  while (f.available() && _pathCount < MAX_PATH_POINTS) {
    size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n')) line[--n] = '\0';
    if (n == 0) continue;
    if (headerSkip > 0) { headerSkip--; continue; }
    double lat, lng;
    if (!_parseLatLng(line, n, lat, lng)) continue;
    if ((idx++ % step) != 0) continue;
    _pathLat[_pathCount] = (float)lat;
    _pathLng[_pathCount] = (float)lng;
    _pathCount++;
  }
  f.close();

  _centerLat = sumLat / total;
  _centerLng = sumLng / total;
  _pathLoaded = true;
}

void WardriveMapView::_recenterOnPath() {
  if (_pathCount == 0) return;
  float minLat = _pathLat[0], maxLat = _pathLat[0];
  float minLng = _pathLng[0], maxLng = _pathLng[0];
  for (uint16_t i = 1; i < _pathCount; i++) {
    if (_pathLat[i] < minLat) minLat = _pathLat[i];
    if (_pathLat[i] > maxLat) maxLat = _pathLat[i];
    if (_pathLng[i] < minLng) minLng = _pathLng[i];
    if (_pathLng[i] > maxLng) maxLng = _pathLng[i];
  }
  _centerLat = (minLat + maxLat) / 2.0;
  _centerLng = (minLng + maxLng) / 2.0;
}

int32_t WardriveMapView::_panStep() const {
  int32_t step = (int32_t)_bw / 2;
  if (step < 32) step = 32;
  return step;
}

// ── Tile fetching / caching ──────────────────────────────

bool WardriveMapView::_ensureTile(IStorage* storage, int tx, int ty, String& outPath) {
  outPath = String(TILE_CACHE_DIR) + "/" + ZOOM + "/" + tx + "/" + ty + ".jpg";
  if (storage->exists(outPath.c_str())) return true;

  storage->makeDir(TILE_CACHE_DIR);
  String d = String(TILE_CACHE_DIR) + "/" + ZOOM;
  storage->makeDir(d.c_str());
  d += "/"; d += tx;
  storage->makeDir(d.c_str());

  return _fetchTile(tx, ty, storage, outPath);
}

bool WardriveMapView::_fetchTile(int tx, int ty, IStorage* storage, const String& outPath) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = String(TILE_URL_PREFIX) + ZOOM + "/" + ty + "/" + tx;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  http.addHeader("User-Agent", "UniGeek");

  int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  fs::File f = storage->open(outPath.c_str(), FILE_WRITE);
  if (!f) { http.end(); return false; }

  int written = http.writeToStream(&f);
  f.close();
  http.end();

  if (written <= 0) {
    storage->deleteFile(outPath.c_str());
    return false;
  }
  return true;
}

// ── TJpg_Decoder callback (clips to body rect) ───────────

bool WardriveMapView::_tileCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (!g_mapInstance) return false;
  auto& lcd = Uni.Lcd;

  int16_t bx = g_mapInstance->_bx;
  int16_t by = g_mapInstance->_by;
  int16_t bw = g_mapInstance->_bw;
  int16_t bh = g_mapInstance->_bh;

  int16_t cx0 = max(x, bx);
  int16_t cy0 = max(y, by);
  int16_t cx1 = min<int16_t>((int16_t)(x + w), (int16_t)(bx + bw));
  int16_t cy1 = min<int16_t>((int16_t)(y + h), (int16_t)(by + bh));
  if (cx0 >= cx1 || cy0 >= cy1) return true;

  int16_t cw = cx1 - cx0;
  int16_t ch = cy1 - cy0;
  if (cw == (int16_t)w && ch == (int16_t)h) {
    lcd.pushImage(x, y, w, h, bitmap);
    return true;
  }
  int16_t skipL = cx0 - x;
  int16_t skipT = cy0 - y;
  for (int16_t row = 0; row < ch; row++) {
    uint16_t* lineBuf = bitmap + (skipT + row) * w + skipL;
    lcd.pushImage(cx0, cy0 + row, cw, 1, lineBuf);
  }
  return true;
}

// ── Cohen–Sutherland line clipping ───────────────────────

static uint8_t outcode(int16_t x, int16_t y, int16_t xmin, int16_t ymin, int16_t xmax, int16_t ymax) {
  uint8_t code = 0;
  if (x < xmin) code |= 1;
  else if (x > xmax) code |= 2;
  if (y < ymin) code |= 4;
  else if (y > ymax) code |= 8;
  return code;
}

void WardriveMapView::_drawClippedLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t xmin = _bx, ymin = _by;
  int16_t xmax = _bx + _bw - 1;
  int16_t ymax = _by + _bh - 1;

  uint8_t out0 = outcode(x0, y0, xmin, ymin, xmax, ymax);
  uint8_t out1 = outcode(x1, y1, xmin, ymin, xmax, ymax);

  while (true) {
    if (!(out0 | out1)) {
      Uni.Lcd.drawLine(x0, y0, x1, y1, color);
      return;
    }
    if (out0 & out1) return;

    uint8_t out = out0 ? out0 : out1;
    int16_t x = 0, y = 0;
    if (out & 8) {
      x = x0 + (int32_t)(x1 - x0) * (ymax - y0) / (y1 - y0);
      y = ymax;
    } else if (out & 4) {
      x = x0 + (int32_t)(x1 - x0) * (ymin - y0) / (y1 - y0);
      y = ymin;
    } else if (out & 2) {
      y = y0 + (int32_t)(y1 - y0) * (xmax - x0) / (x1 - x0);
      x = xmax;
    } else if (out & 1) {
      y = y0 + (int32_t)(y1 - y0) * (xmin - x0) / (x1 - x0);
      x = xmin;
    }
    if (out == out0) { x0 = x; y0 = y; out0 = outcode(x0, y0, xmin, ymin, xmax, ymax); }
    else             { x1 = x; y1 = y; out1 = outcode(x1, y1, xmin, ymin, xmax, ymax); }
  }
}

// ── Drawing ──────────────────────────────────────────────

void WardriveMapView::_drawTiles() {
  auto& lcd = Uni.Lcd;

  int32_t viewCx = _lng2px(_centerLng);
  int32_t viewCy = _lat2py(_centerLat);
  int32_t worldOriginX = viewCx - _bw / 2;
  int32_t worldOriginY = viewCy - _bh / 2;

  int32_t minTx = worldOriginX / TILE_SIZE;
  int32_t maxTx = (worldOriginX + _bw - 1) / TILE_SIZE;
  int32_t minTy = worldOriginY / TILE_SIZE;
  int32_t maxTy = (worldOriginY + _bh - 1) / TILE_SIZE;
  int32_t tileMax = (1 << ZOOM) - 1;

  lcd.fillRect(_bx, _by, _bw, _bh, TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(_tileCallback);

  bool anyMissing = false;
  for (int32_t ty = minTy; ty <= maxTy; ty++) {
    for (int32_t tx = minTx; tx <= maxTx; tx++) {
      if (tx < 0 || ty < 0 || tx > tileMax || ty > tileMax) continue;
      String path;
      if (!_ensureTile(_storage, (int)tx, (int)ty, path)) { anyMissing = true; continue; }
      int16_t sx = (int16_t)(_bx + (tx * TILE_SIZE - worldOriginX));
      int16_t sy = (int16_t)(_by + (ty * TILE_SIZE - worldOriginY));
      TJpgDec.drawFsJpg(sx, sy, path, _storage->getFS());
    }
  }

  if (anyMissing) {
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.setTextDatum(TL_DATUM);
    lcd.drawString("Some tiles failed", _bx + 4, _by + 4);
  }
}

void WardriveMapView::_drawPath() {
  if (_pathCount == 0) return;
  auto& lcd = Uni.Lcd;

  int32_t viewCx = _lng2px(_centerLng);
  int32_t viewCy = _lat2py(_centerLat);
  int32_t worldOriginX = viewCx - _bw / 2;
  int32_t worldOriginY = viewCy - _bh / 2;

  auto toScreen = [&](uint16_t i, int16_t& sx, int16_t& sy) {
    int32_t px = _lng2px(_pathLng[i]);
    int32_t py = _lat2py(_pathLat[i]);
    int32_t dx = px - worldOriginX + _bx;
    int32_t dy = py - worldOriginY + _by;
    if (dx >  32000) dx =  32000;
    if (dx < -32000) dx = -32000;
    if (dy >  32000) dy =  32000;
    if (dy < -32000) dy = -32000;
    sx = (int16_t)dx;
    sy = (int16_t)dy;
  };

  // Thick yellow path — 3 parallel strokes for visible width.
  int16_t px, py, cx, cy;
  toScreen(0, px, py);
  for (uint16_t i = 1; i < _pathCount; i++) {
    toScreen(i, cx, cy);
    _drawClippedLine(px, py, cx, cy, TFT_YELLOW);
    _drawClippedLine((int16_t)(px + 1), py, (int16_t)(cx + 1), cy, TFT_YELLOW);
    _drawClippedLine(px, (int16_t)(py + 1), cx, (int16_t)(cy + 1), TFT_YELLOW);
    px = cx; py = cy;
  }

  int16_t sx, sy;
  toScreen(0, sx, sy);
  if (sx >= _bx && sx < _bx + _bw && sy >= _by && sy < _by + _bh) {
    lcd.fillCircle(sx, sy, 3, TFT_GREEN);
  }
  toScreen(_pathCount - 1, sx, sy);
  if (sx >= _bx && sx < _bx + _bw && sy >= _by && sy < _by + _bh) {
    lcd.fillCircle(sx, sy, 3, TFT_RED);
  }
}

void WardriveMapView::_drawAxisHud() {
  auto& lcd = Uni.Lcd;
  const char* label = (_axis == AXIS_NS) ? " N/S " : " E/W ";

  int16_t fh = lcd.fontHeight();
  int16_t pad = 2;
  int16_t tw = lcd.textWidth(label);
  int16_t bx = _bx + _bw - tw - pad;
  int16_t by = _by + pad;
  lcd.fillRect(bx, by, tw, fh, TFT_BLACK);
  lcd.drawRect(bx, by, tw, fh, TFT_WHITE);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextDatum(TL_DATUM);
  lcd.drawString(label, bx, by);
}

// ── Public API ───────────────────────────────────────────

bool WardriveMapView::init(IStorage* storage, const String& fileName) {
  reset();
  _storage = storage;
  _axis = AXIS_NS;
  g_mapInstance = this;

  if (!storage || !storage->isAvailable()) return false;
  String fullPath = String(WigleUtil::WARDRIVE_PATH) + "/" + fileName;
  if (!storage->exists(fullPath.c_str())) return false;

  ShowStatusAction::show("Loading path...", 0);
  _loadPath(fullPath);

  if (!_pathLoaded) {
    _centerLat = 0.0;
    _centerLng = 0.0;
  }
  return _pathLoaded;
}

void WardriveMapView::reset() {
  _storage    = nullptr;
  _pathLoaded = false;
  _pathCount  = 0;
  _axis       = AXIS_NS;
  if (g_mapInstance == this) g_mapInstance = nullptr;
}

WardriveMapView::NavResult WardriveMapView::onNav(INavigation::Direction dir) {
  auto moveLng = [&](int32_t dxPx) {
    double worldSize = (double)(256 << ZOOM);
    _centerLng += dxPx * 360.0 / worldSize;
    if (_centerLng >  180.0) _centerLng -= 360.0;
    if (_centerLng < -180.0) _centerLng += 360.0;
  };
  auto moveLat = [&](int32_t dyPx) {
    int32_t cy = _lat2py(_centerLat) + dyPx;
    double worldSize = (double)(256 << ZOOM);
    double n = M_PI * (1.0 - 2.0 * cy / worldSize);
    double lat = atan(sinh(n)) * 180.0 / M_PI;
    if (lat > 85.0) lat = 85.0;
    if (lat < -85.0) lat = -85.0;
    _centerLat = lat;
  };

  int32_t step = _panStep();

  if (Uni.Nav->is4Way()) {
    switch (dir) {
      case INavigation::DIR_LEFT:  moveLng(-step); return NAV_HANDLED;
      case INavigation::DIR_RIGHT: moveLng(step);  return NAV_HANDLED;
      case INavigation::DIR_UP:    moveLat(-step); return NAV_HANDLED;
      case INavigation::DIR_DOWN:  moveLat(step);  return NAV_HANDLED;
      case INavigation::DIR_PRESS: _recenterOnPath(); return NAV_HANDLED;
      default: return NAV_IGNORED;
    }
  }
  switch (dir) {
    case INavigation::DIR_PRESS:
      // Axis toggle doesn't move the view — only the HUD chip changes.
      // Redraw it in place and tell the parent to skip a full re-render
      // so we don't refetch / re-decode tiles for a label flip.
      _axis = (Axis)((_axis + 1) % AXIS_COUNT);
      _drawAxisHud();
      return NAV_HUD_ONLY;
    case INavigation::DIR_UP:
      if (_axis == AXIS_NS) moveLat(-step); else moveLng(-step);
      return NAV_HANDLED;
    case INavigation::DIR_DOWN:
      if (_axis == AXIS_NS) moveLat(step);  else moveLng(step);
      return NAV_HANDLED;
    default: return NAV_IGNORED;
  }
}

void WardriveMapView::render(int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  _bx = bx; _by = by; _bw = bw; _bh = bh;
  g_mapInstance = this;
  _drawTiles();
  _drawPath();
  if (!Uni.Nav->is4Way()) _drawAxisHud();
}
