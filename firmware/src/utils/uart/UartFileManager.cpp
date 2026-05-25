#include "utils/uart/UartFileManager.h"
#include "core/Device.h"
#include "FirmwareVersion.h"

UartFileManager UartFM;

static uint32_t _crc32(const uint8_t* hdr, uint32_t hdrLen, const uint8_t* data, uint32_t dataLen) {
  uint32_t crc = 0xFFFFFFFF;
  for (uint32_t i = 0; i < hdrLen; i++) {
    crc ^= hdr[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
  }
  for (uint32_t i = 0; i < dataLen; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
  }
  return ~crc;
}

void UartFileManager::begin() {
  // Serial.begin() runs in setup() before us; nothing to do here yet.
}

void UartFileManager::update() {
  while (Serial.available() > 0) {
    _onByte((uint8_t)Serial.read());
  }
}

void UartFileManager::_onByte(uint8_t b) {
  switch (_state) {
    case State::WaitSof1:
      if (b == SOF1) _state = State::WaitSof2;
      break;
    case State::WaitSof2:
      if (b == SOF2)       { _state = State::ReadHeader; _headerIdx = 0; }
      else if (b != SOF1)  { _state = State::WaitSof1; }
      break;
    case State::ReadHeader:
      _headerBuf[_headerIdx++] = b;
      if (_headerIdx == 7) {
        _ctx  = _headerBuf[0];
        _type = _headerBuf[1];
        _seq  = _headerBuf[2];
        _len  = (uint32_t)_headerBuf[3]
              | ((uint32_t)_headerBuf[4] << 8)
              | ((uint32_t)_headerBuf[5] << 16)
              | ((uint32_t)_headerBuf[6] << 24);
        if (_len > MAX_PAYLOAD) { _state = State::WaitSof1; break; }
        _payloadIdx = 0;
        _crcIdx     = 0;
        _state      = (_len > 0) ? State::ReadPayload : State::ReadCrc;
      }
      break;
    case State::ReadPayload:
      _payload[_payloadIdx++] = b;
      if (_payloadIdx >= _len) _state = State::ReadCrc;
      break;
    case State::ReadCrc:
      _crcBuf[_crcIdx++] = b;
      if (_crcIdx == 4) {
        uint32_t expected = (uint32_t)_crcBuf[0]
                          | ((uint32_t)_crcBuf[1] << 8)
                          | ((uint32_t)_crcBuf[2] << 16)
                          | ((uint32_t)_crcBuf[3] << 24);
        uint32_t actual   = _crc32(_headerBuf, 7, _payload, _len);
        if (actual == expected) _dispatch();
        _state = State::WaitSof1;
      }
      break;
  }
}

void UartFileManager::_dispatch() {
  _payload[_len] = 0; // safe null-terminator for string commands

  switch (_ctx) {
    case CTX_FM: _dispatchFm(); break;
    default:     _sendErr(_seq, "unknown context");
  }
}

void UartFileManager::_dispatchFm() {
  switch (_type) {
    case C_INFO:      _handleInfo(_seq); break;
    case C_LS:        _handleLs(_seq, (const char*)_payload); break;
    case C_STAT:      _handleStat(_seq, (const char*)_payload); break;
    case C_GET:       _handleGet(_seq, (const char*)_payload); break;
    case C_PUT_BEGIN: _handlePutBegin(_seq, _payload, _len); break;
    case C_PUT_CHUNK: _handlePutChunk(_seq, _payload, _len); break;
    case C_PUT_END:   _handlePutEnd(_seq); break;
    case C_RM:        _handleRm(_seq, (const char*)_payload); break;
    case C_MV:        _handleMv(_seq, _payload, _len); break;
    case C_MKDIR:     _handleMkdir(_seq, (const char*)_payload); break;
    case C_TOUCH:     _handleTouch(_seq, (const char*)_payload); break;
    default:          _sendErr(_seq, "unknown command");
  }
}

void UartFileManager::_sendFrame(uint8_t ctx, uint8_t type, uint8_t seq, const uint8_t* data, uint32_t len) {
  uint8_t hdr[9];
  hdr[0] = SOF1; hdr[1] = SOF2;
  hdr[2] = ctx;  hdr[3] = type; hdr[4] = seq;
  hdr[5] = (uint8_t)(len);
  hdr[6] = (uint8_t)(len >> 8);
  hdr[7] = (uint8_t)(len >> 16);
  hdr[8] = (uint8_t)(len >> 24);

  uint32_t crc = _crc32(hdr + 2, 7, data, len);
  uint8_t  crcb[4] = {
    (uint8_t)(crc),
    (uint8_t)(crc >> 8),
    (uint8_t)(crc >> 16),
    (uint8_t)(crc >> 24),
  };

  Serial.write(hdr, 9);
  if (len) Serial.write(data, len);
  Serial.write(crcb, 4);
}

void UartFileManager::_sendOk(uint8_t seq) {
  _sendFrame(_ctx, T_OK, seq, nullptr, 0);
}

void UartFileManager::_sendErr(uint8_t seq, const char* msg) {
  size_t L = msg ? strlen(msg) : 0;
  _sendFrame(_ctx, T_ERR, seq, (const uint8_t*)msg, L);
}

void UartFileManager::_handleInfo(uint8_t seq) {
  uint64_t total = (Uni.Storage && Uni.Storage->isAvailable()) ? Uni.Storage->totalBytes() : 0;
  uint64_t used  = (Uni.Storage && Uni.Storage->isAvailable()) ? Uni.Storage->usedBytes()  : 0;
  uint32_t heap  = (uint32_t)ESP.getFreeHeap();
  char buf[256];
  int n = snprintf(buf, sizeof(buf),
    "{\"name\":\"UniGeek\",\"version\":\"%s\",\"total\":%llu,\"used\":%llu,\"heap\":%u}",
    FIRMWARE_VERSION,
    (unsigned long long)total,
    (unsigned long long)used,
    (unsigned)heap);
  if (n < 0) n = 0;
  if ((size_t)n > sizeof(buf)) n = sizeof(buf);
  _sendFrame(_ctx, T_OK, seq, (const uint8_t*)buf, (uint32_t)n);
}

void UartFileManager::_handleLs(uint8_t seq, const char* path) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  String p = (path && *path) ? String(path) : String("/");
  fs::File dir = Uni.Storage->open(p.c_str(), FILE_READ);
  if (!dir || !dir.isDirectory()) { _sendErr(seq, "not a directory"); return; }

  String resp;
  resp.reserve(1024);
  while (true) {
    fs::File f = dir.openNextFile();
    if (!f) break;
    // Stop adding entries if the next one would overflow the frame buffer.
    String row = (f.isDirectory() ? "DIR:" : "FILE:");
    row += String(f.name());
    row += ":";
    row += String(f.size());
    row += "\n";
    if (resp.length() + row.length() > MAX_PAYLOAD - 64) { f.close(); break; }
    resp += row;
    f.close();
  }
  dir.close();
  _sendFrame(_ctx, T_OK, seq, (const uint8_t*)resp.c_str(), resp.length());
}

void UartFileManager::_handleStat(uint8_t seq, const char* path) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  if (!path || !*path) { _sendErr(seq, "no path"); return; }
  if (!Uni.Storage->exists(path)) { _sendErr(seq, "not found"); return; }
  fs::File f = Uni.Storage->open(path, FILE_READ);
  if (!f) { _sendErr(seq, "open failed"); return; }
  bool   isDir = f.isDirectory();
  size_t sz    = isDir ? 0 : f.size();
  f.close();
  char buf[64];
  int  n = snprintf(buf, sizeof(buf), "%s:%u", isDir ? "DIR" : "FILE", (unsigned)sz);
  _sendFrame(_ctx, T_OK, seq, (const uint8_t*)buf, (uint32_t)n);
}

void UartFileManager::_handleGet(uint8_t seq, const char* path) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  if (!path || !*path)                              { _sendErr(seq, "no path");    return; }
  if (!Uni.Storage->exists(path))                   { _sendErr(seq, "not found");  return; }

  fs::File f = Uni.Storage->open(path, FILE_READ);
  if (!f)              { _sendErr(seq, "open failed"); return; }
  if (f.isDirectory()) { f.close(); _sendErr(seq, "is directory"); return; }

  static uint8_t buf[2048];
  while (true) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    _sendFrame(_ctx, T_GET_CHUNK, seq, buf, (uint32_t)n);
  }
  f.close();
  _sendFrame(_ctx, T_GET_CHUNK, seq, nullptr, 0); // zero-length = end of stream
}

void UartFileManager::_handlePutBegin(uint8_t seq, const uint8_t* payload, uint32_t len) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  // payload: [total_size:u32 LE][path utf-8]. _dispatch() already terminated
  // _payload[len] with a null byte, so reading as a C-string is safe.
  if (len < 5) { _sendErr(seq, "bad payload"); return; }
  String path((const char*)(payload + 4));
  if (path.length() == 0) { _sendErr(seq, "no path"); return; }

  int sl = path.lastIndexOf('/');
  if (sl > 0) Uni.Storage->makeDir(path.substring(0, sl).c_str());

  if (_putFile) _putFile.close();
  _putFile = Uni.Storage->open(path.c_str(), FILE_WRITE);
  if (!_putFile) { _putActive = false; _sendErr(seq, "open failed"); return; }
  _putPath   = path;
  _putActive = true;
  _sendOk(seq);
}

void UartFileManager::_handlePutChunk(uint8_t seq, const uint8_t* data, uint32_t len) {
  if (!_putActive || !_putFile) { _sendErr(seq, "no upload active"); return; }
  size_t w = _putFile.write(data, len);
  if (w != len) { _sendErr(seq, "short write"); return; }
  _sendOk(seq);
}

void UartFileManager::_handlePutEnd(uint8_t seq) {
  if (_putFile) _putFile.close();
  _putActive = false;
  _putPath   = "";
  _sendOk(seq);
}

bool UartFileManager::_removeDirectory(const String& path) {
  fs::File dir = Uni.Storage->open(path.c_str(), FILE_READ);
  if (!dir || !dir.isDirectory()) return false;
  fs::File f = dir.openNextFile();
  while (f) {
    String fp = String(f.path());
    if (f.isDirectory()) _removeDirectory(fp);
    else                 Uni.Storage->deleteFile(fp.c_str());
    f = dir.openNextFile();
  }
  dir.close();
  return Uni.Storage->removeDir(path.c_str());
}

void UartFileManager::_handleRm(uint8_t seq, const char* path) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  if (!path || !*path)                              { _sendErr(seq, "no path");    return; }
  if (!Uni.Storage->exists(path))                   { _sendErr(seq, "not found");  return; }
  fs::File f     = Uni.Storage->open(path, FILE_READ);
  bool     isDir = f && f.isDirectory();
  if (f) f.close();
  bool ok = isDir ? _removeDirectory(String(path)) : Uni.Storage->deleteFile(path);
  ok ? _sendOk(seq) : _sendErr(seq, "delete failed");
}

void UartFileManager::_handleMv(uint8_t seq, const uint8_t* payload, uint32_t len) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  // payload: src\0dst (the trailing null after dst was added by _dispatch).
  uint32_t i = 0;
  while (i < len && payload[i] != 0) i++;
  if (i >= len) { _sendErr(seq, "bad payload"); return; }
  String src((const char*)payload);
  String dst((const char*)(payload + i + 1));
  if (src.length() == 0 || dst.length() == 0) { _sendErr(seq, "bad src/dst"); return; }
  if (!Uni.Storage->exists(src.c_str()))      { _sendErr(seq, "src not found"); return; }
  bool ok = Uni.Storage->renameFile(src.c_str(), dst.c_str());
  ok ? _sendOk(seq) : _sendErr(seq, "rename failed");
}

void UartFileManager::_handleMkdir(uint8_t seq, const char* path) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  if (!path || !*path)                              { _sendErr(seq, "no path");    return; }
  bool ok = Uni.Storage->makeDir(path);
  ok ? _sendOk(seq) : _sendErr(seq, "mkdir failed");
}

void UartFileManager::_handleTouch(uint8_t seq, const char* path) {
  if (!Uni.Storage || !Uni.Storage->isAvailable()) { _sendErr(seq, "no storage"); return; }
  if (!path || !*path)                              { _sendErr(seq, "no path");    return; }
  fs::File f = Uni.Storage->open(path, FILE_WRITE);
  if (!f) { _sendErr(seq, "open failed"); return; }
  f.close();
  _sendOk(seq);
}
