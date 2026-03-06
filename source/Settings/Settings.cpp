#include "Settings.h"

void Settings::checkUpdates()
{
  MVP& m = mvp.get();
  if (m.translation != prevTranslation ||
      m.rotation    != prevRotation    ||
      m.scale       != prevScale)
  {
    prevTranslation = m.translation;
    prevRotation    = m.rotation;
    prevScale       = m.scale;
    mvp.modify().setModel();
    mvp.notify();
  }

  if (clearColor != prevClearColor)
  {
    clearColorNeedsUpdate = true;
    prevClearColor  = clearColor;
  }
}
