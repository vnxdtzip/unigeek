//
// MapPickerUtil — shared precondition check before showing the wardrive
// map picker. Both WigleScreen and GPSScreen call ensureWifi() before
// listing files.
//

#pragma once

#include <Arduino.h>

class MapPickerUtil
{
public:
  // Returns true if WiFi is connected. Otherwise shows a status popup
  // ("Connect to internet first") and returns false.
  static bool ensureWifi();
};
