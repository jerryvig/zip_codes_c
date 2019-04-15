## Depedencies

The following libraries are required to build this project:

* cURL - https://curl.haxx.se/download.html
* sqlite3 - https://www.sqlite.org/download.html
* Simple Dynamic Strings (libsds) by [antirez](https://github.com/antirez) (The creator of Redis) - https://github.com/antirez/sds

## Building with Ninja

```
$ mkdir build
$ cd build
$ cmake -G Ninja ../
$ ninja -C .
```
