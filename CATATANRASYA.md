# Catatan Pengembangan v1.0.1(F) "Fixed"

**Tanggal:** 9 Maret 2026
**Oleh:** Rasya Andrean

---

## Apa yang Terjadi di Versi Ini

Jadi ceritanya, setelah v1.0.0 rilis tanggal 2 Maret, ada kontributor bernama **John-fried** yang aktif banget ngirim PR dari tanggal 7 sampai 9 Maret. Total ada 5 PR (#2 sampai #7) yang dia kirim, dan semuanya ngebantu banget buat ngestabilin compiler.

PR pertamanya (#2) itu fix buat Linux. Ternyata `urus_runtime.h` kita kurang `#include <stddef.h>`, jadi waktu compile di Linux langsung error. Selain itu ada juga masalah implicit declaration fungsi POSIX karena kita pakai C11 strict. Hal-hal kecil tapi krusial banget kalau mau project-nya jalan lintas platform.

Terus dia lanjut bikin PR #3 yang nambahin `install` sama `uninstall` target di Makefile. Simpel tapi berguna, jadi user bisa langsung `make install` tanpa ribet copy manual.

PR #4 dia nambahin fungsi `show_help()` di `main.c`. Sebelumnya kalau user jalanin `urusc` tanpa argumen, cuma keluar error doang, nggak ada petunjuk cara pakainya. Sekarang udah ada info lengkap soal options yang tersedia.

Nah, yang paling gede perubahannya itu PR #5 — migrasi dari Makefile ke **CMake**. Ini lumayan major karena sebelumnya kita punya `Makefile` buat Linux/macOS dan `build.bat` buat Windows. Dua file terpisah yang harus di-maintain. Dengan CMake, satu `CMakeLists.txt` bisa handle semua platform. Dia juga bikin script `cmake/embed-string.cmake` yang tugasnya ngubah `urus_runtime.h` jadi C array yang di-embed langsung ke binary compiler.

PR #6 itu lanjutan dari embed tadi — sekarang compiler jadi **standalone**. Artinya user nggak perlu lagi simpen file `urus_runtime.h` di samping compiler. Runtime header-nya udah nempel di dalam binary, jadi tinggal copy satu file `urusc.exe` aja udah cukup. Oh iya, di PR ini dia juga fix bug string yang nggak di-terminate di fungsi `emit()` di codegen.

Terakhir PR #7. Ini soal **error reporting**. Sebelumnya kalau ada error, pesan yang keluar cuma kayak "Error at line 5" gitu doang. Sekarang udah ada warna (merah buat "Error", hijau buat caret), nama file, nomor baris, bahkan nunjukin baris kode yang bermasalah lengkap sama tanda panah (^) yang nunjuk tepat ke kolom mana errornya. Jauh lebih gampang buat debugging.

---

## Masalah yang Ditemukan Waktu Testing

Setelah semua PR itu di-merge, mulai testing buat rilis v1.0.1. Dan ternyata banyak yang pecah, terutama di Windows. Ini wajar karena John-fried develop-nya di Linux, jadi beberapa hal yang jalan mulus di sana ternyata bermasalah di Windows.

### 1. `error.c` pakai `getline()` — nggak ada di Windows

Fungsi `getline()` itu POSIX, dan MSVC nggak punya. Waktu build pakai Visual Studio langsung error linking: `unresolved external symbol getline`. Solusinya diganti pakai `fgets()` yang portable dan kerja di semua platform. Sekalian diperbaiki juga memory management-nya — versi lama pakai `malloc`/`free` buat buffer, sekarang pakai stack buffer `char[4096]` yang lebih simpel dan nggak perlu free.

### 2. Generated C file corrupt di Windows (double CRLF)

Ini bug yang lumayan tricky. Jadi ceritanya, `urus_runtime.h` di Windows disimpan pakai line ending CRLF (`\r\n`). Waktu di-embed ke binary, byte-byte `\r\n` itu ikut masuk. Terus waktu compiler nulis file `_urus_tmp.c` pakai `fopen("w")` (text mode), Windows otomatis nambah `\r` lagi di depan setiap `\n`. Jadinya `\r\n` berubah jadi `\r\r\n`. GCC bingung, line number-nya jadi kacau (file 483 baris tapi GCC laporin error di baris 557). Fix-nya simpel — buka file pakai `"wb"` (binary mode) biar Windows nggak ikut campur urusan line ending.

### 3. `--help` dan `--version` dianggap nama file

Logika parsing argumen di `main.c` langsung anggap `argv[1]` itu path file. Jadi kalau user ketik `urusc --help`, compiler malah coba buka file bernama `--help` dan keluar error "cannot open file '--help'". Fix-nya ditambahin loop yang scan semua argumen dulu sebelum assume argv[1] itu file. Sekalian ditambahin juga flag `--version` / `-v` yang belum ada sebelumnya.

### 4. GCC nggak bisa nemuin `cc1` di Windows

Yang ini agak annoying. Compiler kita cari GCC di path hardcoded kayak `C:/msys64/mingw64/bin/gcc.exe`. Ketemu, oke. Tapi waktu GCC dijalanin, dia butuh `cc1` (C compiler backend) yang ada di subdirectory-nya. Karena PATH environment nggak include directory GCC, `cc1` nggak ketemu dan error: `fatal error: cannot execute 'cc1'`. Solusinya: sebelum manggil GCC, compiler sekarang extract directory dari path GCC dan inject ke PATH environment variable.

### 5. Flag `-I include` masih dipakai padahal udah nggak perlu

Setelah PR #6 bikin compiler standalone (runtime di-embed ke generated C), flag `-I include` yang dikirim ke GCC jadi nggak berguna. Lebih parah lagi, path yang dibangun (`build/Release/include`) bahkan nggak exist. Dihapus semua logika `include_dir` dan flag `-I` dari invokasi GCC.

### 6. MSVC warnings soal `strdup`

Build pakai MSVC kasih banyak warning: "strdup deprecated, use _strdup". Ini karena MSVC pengen kita pakai versi dengan underscore prefix. Daripada ganti semua `strdup` di codebase (yang bakal break di GCC/Linux), ditambahin `_CRT_SECURE_NO_WARNINGS` dan `_CRT_NONSTDC_NO_DEPRECATE` di CMakeLists.txt khusus untuk MSVC.

---

## Hasil Test Akhir

Setelah semua fix di atas, rebuild dari nol dan jalanin semua test:

- **10 test cases** di `tests/run/` — semua PASS, output sesuai `.expected`
- **8 contoh program** di `examples/` — semua compile dan jalan normal
- **8 file invalid** di `tests/invalid/` — semua error reporting-nya benar, nunjukin baris dan kolom yang tepat
- **CLI flags** (`--help`, `--version`, `--tokens`, `--ast`, `--emit-c`, `-o`) — semua berfungsi

Zero failures.

---

## File yang Diubah

Total **11 file** yang kena perubahan di versi ini:

| File | Apa yang Berubah |
|------|-----------------|
| `CHANGELOG.md` | Tambah section v1.0.1(F) lengkap |
| `README.md` | Rewrite total — tata letak baru, struktur folder akurat, URL repo updated |
| `SPEC.md` | Update version header |
| `CONTRIBUTING.md` | Build instructions ganti ke CMake |
| `SECURITY.md` | Tambah v1.0.1 ke supported versions |
| `Dockerfile` | Ganti ke CMake build, update label org |
| `documentation/installation/README.md` | Requirements: Make diganti CMake |
| `compiler/CMakeLists.txt` | Bump version, tambah MSVC compat defines |
| `compiler/src/main.c` | Fix --help/--version, fix GCC PATH, fix binary mode, hapus -I |
| `compiler/src/error.c` | Ganti getline() ke fgets() |
| `compiler/src/codegen.c` | Update version string di generated code |

---

## Pelajaran

Kalau ada satu hal yang dipelajari dari rilis ini: **selalu test di semua platform target**. Banyak bug di atas nggak bakal ketemu kalau cuma test di Linux. Windows punya behavior beda di soal line ending, PATH resolution, dan fungsi POSIX. Cross-platform itu bukan cuma soal "compile di mana-mana", tapi juga soal "jalan bener di mana-mana".

Juga, bikin compiler standalone (embed runtime ke binary) itu keputusan bagus. Sebelumnya user harus manage dua file (compiler + runtime header), sekarang cukup satu file executable. Lebih gampang distribute, lebih gampang install.

---

*v1.0.1(F) "Fixed" — karena memang isinya fixing semua hal yang pecah setelah batch contribution pertama.*
