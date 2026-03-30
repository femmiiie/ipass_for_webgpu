#pragma once

#include <functional>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <nfd.hpp>
#endif

namespace utils
{
  //callback receives the selected file path on both platforms.
  //on web, the file is written to emscripten's virtual FS at /tmp/<name> before the callback fires.
  //the callback signature is void(std::string path).
  using Callback = std::function<void(std::string)>;



  inline void OpenFile(const char* description, const char* extension, Callback callback)
  {
  //web requires async access for file dialogs, impossible to have a shared implementation
  // #ifdef __EMSCRIPTEN__
    //todo add file opening for emscripten

  // #else
    NFD::Guard guard;
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{ description, extension }};
    nfdresult_t result = NFD::OpenDialog(path, filters, 1);
    if (result == NFD_OKAY)
    {
      callback(std::string(path.get()));
    }
  // #endif
  }
}