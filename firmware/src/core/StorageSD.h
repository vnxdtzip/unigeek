//
// Created by L Shaf on 2026-02-23.
//

#pragma once

#include "core/IStorage.h"
#include "core/GuardedSdFs.h"
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>

class StorageSD : public IStorage
{
public:
  // dcPin: shared DC/MISO pin (e.g. CoreS3 GPIO35). Pass -1 if not shared.
  bool begin(uint8_t csPin, SPIClass& spi, uint32_t freq = 4000000, int8_t dcPin = -1) {
    _csPin = csPin;
    _freq  = freq;
    _spi   = &spi;
    _guard.init(&spi, dcPin);
    {
      MisoDcGuard::Scope s(_guard);
      _available = SD.begin(csPin, spi, freq);
    }
    if (_available) _guardedFs = makeGuardedFs(SD, _guard);
    return _available;
  }

  bool isAvailable() override { return _available; }

  uint64_t totalBytes() override {
    if (!_available) return 0;
    MisoDcGuard::Scope s(_guard);
    return SD.totalBytes();
  }
  uint64_t usedBytes() override {
    if (!_available) return 0;
    MisoDcGuard::Scope s(_guard);
    return SD.usedBytes();
  }
  uint64_t freeBytes() override {
    uint64_t t = totalBytes(), u = usedBytes();
    return (u < t) ? (t - u) : 0;
  }

  fs::File open(const char* path, const char* mode) override {
    if (!_available) return fs::File();
    return _guardedFs.open(path, mode);
  }

  bool exists(const char* path) override {
    if (!_available) return false;
    MisoDcGuard::Scope s(_guard);
    return SD.exists(path);
  }

  String readFile(const char* path) override {
    if (!_available) return "";
    MisoDcGuard::Scope s(_guard);
    File f = SD.open(path, FILE_READ);
    if (!f) return "";
    String content = f.readString();
    f.close();
    return content;
  }

  bool writeFile(const char* path, const char* content) override {
    if (!_available) return false;
    const size_t len = strlen(content);
    for (int attempt = 0; attempt < 3; attempt++) {
      bool ok = false;
      {
        MisoDcGuard::Scope s(_guard);
        File f = SD.open(path, FILE_WRITE);
        if (f) {
          size_t written = f.write(reinterpret_cast<const uint8_t*>(content), len);
          f.flush();
          int writeErr = f.getWriteError();
          f.close();
          ok = (written == len && writeErr == 0);
        }
      }
      if (ok) return true;
      delay(5);
      _recover();
    }
    _available = false;
    return false;
  }

  bool deleteFile(const char* path) override {
    if (!_available) return false;
    MisoDcGuard::Scope s(_guard);
    return SD.remove(path);
  }

  bool makeDir(const char* path) override {
    if (!_available) return false;
    MisoDcGuard::Scope s(_guard);
    String p = path;
    for (int i = 1; i < (int)p.length(); i++) {
      if (p[i] == '/') {
        String sub = p.substring(0, i);
        if (!SD.exists(sub.c_str())) SD.mkdir(sub.c_str());
      }
    }
    return SD.exists(path) ? true : SD.mkdir(path);
  }

  bool renameFile(const char* from, const char* to) override {
    if (!_available) return false;
    MisoDcGuard::Scope s(_guard);
    return SD.rename(from, to);
  }

  bool removeDir(const char* path) override {
    if (!_available) return false;
    MisoDcGuard::Scope s(_guard);
    return SD.rmdir(path);
  }

  uint8_t listDir(const char* path, DirEntry* out, uint8_t max) override {
    if (!_available) return 0;
    MisoDcGuard::Scope s(_guard);
    File dir = SD.open(path);
    if (!dir) return 0;
    uint8_t count = 0;
    while (count < max) {
      // Lightweight iteration — no File handle allocated per entry. Same
      // approach Bruce uses; orders of magnitude faster on large directories
      // (e.g. Flipper IRDB folders with 100+ entries) than openNextFile().
      bool   isDir;
      String full = dir.getNextFileName(&isDir);
      if (full.length() == 0) break;
      int slash = full.lastIndexOf('/');
      out[count].name  = (slash >= 0) ? full.substring(slash + 1) : full;
      out[count].isDir = isDir;
      count++;
    }
    dir.close();
    return count;
  }

  fs::FS& getFS() override {
    return _available ? _guardedFs : static_cast<fs::FS&>(SD);
  }

  // ── Raw block-device access (USB Mass Storage) ───────────────────────────
  // SD presents a real FAT volume in 512-byte sectors. Raw reads/writes go
  // through the MISO/DC guard just like every other SD op so concurrent access
  // (USB ISR task vs render loop) is serialised and GPIO35 stays correct on
  // CoreS3-style boards.
  bool isBlockDevice() override { return _available; }

  uint32_t blockCount() override {
    if (!_available) return 0;
    MisoDcGuard::Scope s(_guard);
    return (uint32_t)SD.numSectors();
  }

  uint16_t blockSize() override {
    if (!_available) return 0;
    MisoDcGuard::Scope s(_guard);
    return (uint16_t)SD.sectorSize();
  }

  bool readBlocks(uint32_t lba, uint8_t* dst, uint32_t count) override {
    if (!_available || !dst) return false;
    MisoDcGuard::Scope s(_guard);
    for (uint32_t i = 0; i < count; i++) {
      if (!SD.readRAW(dst + i * SECTOR_SIZE, lba + i)) return false;
    }
    return true;
  }

  bool writeBlocks(uint32_t lba, const uint8_t* src, uint32_t count) override {
    if (!_available || !src) return false;
    MisoDcGuard::Scope s(_guard);
    for (uint32_t i = 0; i < count; i++) {
      // writeRAW takes a non-const buffer but does not modify it.
      if (!SD.writeRAW(const_cast<uint8_t*>(src) + i * SECTOR_SIZE, lba + i)) return false;
    }
    return true;
  }

private:
  static constexpr uint32_t SECTOR_SIZE = 512;  // SD readRAW/writeRAW operate one 512B sector at a time

  bool        _available = false;
  uint8_t     _csPin     = 0;
  uint32_t    _freq      = 4000000;
  SPIClass*   _spi       = nullptr;
  MisoDcGuard _guard;
  fs::FS      _guardedFs{fs::FSImplPtr()};

  void _recover() {
    SD.end();
    delayMicroseconds(500);
    MisoDcGuard::Scope s(_guard);
    _available = SD.begin(_csPin, *_spi, _freq);
  }
};