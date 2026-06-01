//
// Created by L Shaf on 2026-02-23.
//

#pragma once

#include <Arduino.h>
#include <FS.h>

class IStorage
{
public:
  struct DirEntry {
    String name;
    bool   isDir;
  };

  virtual bool     isAvailable()                                          = 0;
  virtual uint64_t totalBytes()                                           = 0;
  virtual uint64_t usedBytes()                                            = 0;
  virtual uint64_t freeBytes()                                            = 0;
  virtual fs::File open(const char* path, const char* mode)               = 0;
  virtual bool    exists(const char* path)                               = 0;
  virtual String  readFile(const char* path)                             = 0;
  virtual bool    writeFile(const char* path, const char* content)       = 0;
  virtual bool    deleteFile(const char* path)                           = 0;
  virtual bool    makeDir(const char* path)                              = 0;
  virtual uint8_t listDir(const char* path, DirEntry* out, uint8_t max)  = 0;
  virtual bool    renameFile(const char* from, const char* to)           = 0;
  virtual bool    removeDir(const char* path)                            = 0;
  virtual fs::FS& getFS()                                                = 0;

  // ── Raw block-device access (used by USB Mass Storage) ───────────────────
  // Only backends that present a real FAT-formatted block device (SD card)
  // override these. The default (e.g. LittleFS) reports "not a block device"
  // so callers can fall back gracefully — LittleFS is not a FAT volume and
  // cannot be exposed to a host as a USB drive.
  virtual bool     isBlockDevice()                                       { return false; }
  virtual uint32_t blockCount()                                          { return 0; }     // number of LBA sectors
  virtual uint16_t blockSize()                                           { return 0; }     // bytes per sector (typically 512)
  virtual bool     readBlocks(uint32_t lba, uint8_t* dst, uint32_t count)        { (void)lba; (void)dst; (void)count; return false; }
  virtual bool     writeBlocks(uint32_t lba, const uint8_t* src, uint32_t count) { (void)lba; (void)src; (void)count; return false; }

  virtual ~IStorage() {}
};