# Executor (Geode, C++)

Oyun içinde C++ kodu derleyip çalıştırır (sadece Windows). Mod yeniden derlemeden test için.

## Gereksinim
- clang++ (LLVM) PATH'te olmalı, MSVC kütüphaneleri (Visual Studio Build Tools) kurulu olmalı
- GD Windows sürümün 32-bit ise bayrak: --target=i686-pc-windows-msvc (varsayılan), 64-bit ise x86_64-pc-windows-msvc
- Ayarlar: mod ayarlarından Derleyici ve Bayraklar değiştirilebilir

## Kullanım
Pause menüsü -> "Exec"
- Run: kutudaki kod run() fonksiyonunun içine sarılır. Örn: api->notify("selam");
- File: Geode save dir / executor / scripts / <isim>.cpp dosyasını derler (örn: example)
- Unload: yüklenen DLL'leri ve onUpdate callback'lerini kaldırır

Dosyalarda şu giriş noktası gerekir:

    #include "exec_api.h"
    EXEC_EXPORT void run(ExecAPI* api) { ... }

## API (exec_api.h)
notify, setSpeed, setFPS, restart, getPos, setPos, levelName, onUpdate

Hata olursa scripts/build/last.log dosyasına bak.
Not: derleme sırasında oyun birkaç saniye donar.
