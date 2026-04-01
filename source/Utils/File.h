#pragma once

#include <functional>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdio>
#else
#include <nfd.hpp>
#endif

namespace utils
{
  //callback receives the selected file path on both platforms.
  //on web, the file is written to emscripten's virtual FS at /tmp/<name> before the callback fires.
  //the callback signature is void(std::string path).
  using Callback = std::function<void(std::string)>;

#ifdef __EMSCRIPTEN__

  namespace detail {
    inline Callback& PendingFileCallback() {
      static Callback cb;
      return cb;
    }
  }

  inline void OpenFile(const char* description, const char* extension, Callback callback)
  {
    (void)description; //remove unused parameter warning
    detail::PendingFileCallback() = std::move(callback);

    EM_ASM({
      var accept = '.' + UTF8ToString($0);
      var input = document.createElement('input');
      input.type = 'file';
      input.accept = accept;
      input.onchange = function(e) {
        var file = e.target.files[0];
        if (!file) return;
        var reader = new FileReader();
        reader.onload = function() {
          var data = new Uint8Array(reader.result);
          var nameLen = lengthBytesUTF8(file.name) + 1;
          var namePtr = _malloc(nameLen);
          stringToUTF8(file.name, namePtr, nameLen);
          var dataPtr = _malloc(data.length);
          HEAPU8.set(data, dataPtr);
          _emOpenFileComplete(namePtr, dataPtr, data.length);
          _free(namePtr);
          _free(dataPtr);
        };
        reader.readAsArrayBuffer(file);
      };
      input.click();
    }, extension);
  }

#else

  inline void OpenFile(const char* description, const char* extension, Callback callback)
  {
    NFD::Guard guard;
    NFD::UniquePath path;
    nfdfilteritem_t filters[] = {{ description, extension }};
    nfdresult_t result = NFD::OpenDialog(path, filters, 1);
    if (result == NFD_OKAY)
    {
      callback(std::string(path.get()));
    }
  }

#endif
}

#ifdef __EMSCRIPTEN__
extern "C" inline EMSCRIPTEN_KEEPALIVE void emOpenFileComplete(const char* filename, const void* data, int dataLen)
{
  std::string path = std::string("/tmp/") + filename;
  FILE* f = fopen(path.c_str(), "wb");
  if (f)
  {
    fwrite(data, 1, (size_t)dataLen, f);
    fclose(f);
  }
  auto& cb = utils::detail::PendingFileCallback();
  if (cb) cb(path);
}
#endif