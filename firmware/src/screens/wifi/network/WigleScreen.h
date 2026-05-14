#pragma once

#include "ui/templates/ListScreen.h"
#include "ui/views/ScrollListView.h"
#include "utils/gps/WigleUtil.h"
#include "utils/gps/WardriveMapView.h"

class WigleScreen : public ListScreen
{
public:
  const char* title() override { return "Wigle"; }

  void onInit() override;
  void onBack() override;
  void onItemSelected(uint8_t index) override;
  void onUpdate() override;
  void onRender() override;

private:
  enum State {
    STATE_MENU,
    STATE_STATS,
    STATE_UPLOAD,
    STATE_MAP_PICK,
    STATE_MAP,
  } _state = STATE_MENU;

  WardriveMapView _map;

  String _tokenSub;

  ListItem _menuItems[4] = {
    {"Wigle Token"},
    {"Wardrive Stat"},
    {"Upload Wardrive"},
    {"Map View"},
  };

  // Stats
  ScrollListView _statsView;
  ScrollListView::Row _statsRows[WigleUtil::MAX_STAT_ROWS];

  // Upload
  ListItem _uploadItems[WigleUtil::MAX_FILES];
  String _fileNames[WigleUtil::MAX_FILES];
  String _fileLabels[WigleUtil::MAX_FILES];
  bool _fileUploaded[WigleUtil::MAX_FILES];
  uint8_t _fileCount = 0;

  void _showMenu();
  void _editToken();
  void _showStats();
  void _showUploadMenu();
  void _uploadFile(uint8_t index);
  void _showMapPickMenu();
  void _openMap(uint8_t index);
};

