#pragma once

// UART file manager — binary-framed protocol over the boot Serial port.
//
// Always-on background service. The website talks to it via Web Serial.
// No password, no on-device menu — purely transport. Frames carry a CRC32
// so stray Serial.print debug output cannot be mistaken for a real frame.
//
// Frame layout (little-endian):
//   [0xA5][0x5A][ctx:1][type:1][seq:1][len:4][payload:len][crc32:4]
// `ctx` namespaces the command so other subsystems (wifi, ble, …) can
// reuse the same transport without colliding with FM command codes. CRC32
// (zlib polynomial 0xEDB88320, init 0xFFFFFFFF, output inverted) covers
// ctx+type+seq+len+payload. Responses echo the request's `ctx`.

#include <Arduino.h>
#include <FS.h>

class UartFileManager {
public:
  void begin();
  void update();

private:
  static constexpr uint8_t  SOF1        = 0xA5;
  static constexpr uint8_t  SOF2        = 0x5A;
  static constexpr uint32_t MAX_PAYLOAD = 8192;

  // Subsystem context. Use ASCII letters so frames are recognizable in a
  // raw serial dump. Other subsystems would pick their own (e.g. 'W' wifi).
  static constexpr uint8_t CTX_FM = 'F';

  // Response types (device -> host) — same codes regardless of ctx.
  static constexpr uint8_t T_OK        = 0xF0;
  static constexpr uint8_t T_ERR       = 0xF1;
  static constexpr uint8_t T_GET_CHUNK = 0xF2;

  // FM command types (host -> device). Only valid when ctx == CTX_FM.
  static constexpr uint8_t C_INFO      = 0x01;
  static constexpr uint8_t C_LS        = 0x02;
  static constexpr uint8_t C_STAT      = 0x03;
  static constexpr uint8_t C_GET       = 0x10;
  static constexpr uint8_t C_PUT_BEGIN = 0x20;
  static constexpr uint8_t C_PUT_CHUNK = 0x21;
  static constexpr uint8_t C_PUT_END   = 0x22;
  static constexpr uint8_t C_RM        = 0x30;
  static constexpr uint8_t C_MV        = 0x31;
  static constexpr uint8_t C_MKDIR     = 0x32;
  static constexpr uint8_t C_TOUCH     = 0x33;

  enum class State : uint8_t { WaitSof1, WaitSof2, ReadHeader, ReadPayload, ReadCrc };

  State    _state      = State::WaitSof1;
  uint8_t  _headerBuf[7]; // ctx, type, seq, len[4]
  uint8_t  _headerIdx  = 0;
  uint8_t  _ctx        = 0;
  uint8_t  _type       = 0;
  uint8_t  _seq        = 0;
  uint32_t _len        = 0;
  uint32_t _payloadIdx = 0;
  uint8_t  _payload[MAX_PAYLOAD + 1]; // +1 for null-terminator after string commands
  uint8_t  _crcBuf[4];
  uint8_t  _crcIdx     = 0;

  // PUT state
  fs::File _putFile;
  String   _putPath;
  bool     _putActive  = false;

  void _onByte(uint8_t b);
  void _dispatch();
  void _dispatchFm();

  void _sendFrame(uint8_t ctx, uint8_t type, uint8_t seq, const uint8_t* data, uint32_t len);
  void _sendOk(uint8_t seq);
  void _sendErr(uint8_t seq, const char* msg);

  void _handleInfo(uint8_t seq);
  void _handleLs(uint8_t seq, const char* path);
  void _handleStat(uint8_t seq, const char* path);
  void _handleGet(uint8_t seq, const char* path);
  void _handlePutBegin(uint8_t seq, const uint8_t* payload, uint32_t len);
  void _handlePutChunk(uint8_t seq, const uint8_t* data, uint32_t len);
  void _handlePutEnd(uint8_t seq);
  void _handleRm(uint8_t seq, const char* path);
  void _handleMv(uint8_t seq, const uint8_t* payload, uint32_t len);
  void _handleMkdir(uint8_t seq, const char* path);
  void _handleTouch(uint8_t seq, const char* path);

  bool _removeDirectory(const String& path);
};

extern UartFileManager UartFM;
