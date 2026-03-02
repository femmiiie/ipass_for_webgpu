#include "Settings.h"

void Settings::checkUpdates()
{
  if (translation != prevTranslation ||
      rotation    != prevRotation    ||
      scale       != prevScale)
  {
    transformNeedsUpdate  = true;
    prevTranslation = translation;
    prevRotation    = rotation;
    prevScale       = scale;
  }

  if (clearColor != prevClearColor)
  {
    clearColorNeedsUpdate = true;
    prevClearColor  = clearColor;
  }
}

bool Settings::resetTransformUpdate()
{
  bool d = transformNeedsUpdate;
  transformNeedsUpdate = false;
  return d;
}

bool Settings::resetClearColorUpdate()
{
  bool d = clearColorNeedsUpdate;
  clearColorNeedsUpdate = false;
  return d;
}
