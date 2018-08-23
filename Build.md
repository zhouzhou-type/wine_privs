# wine_stable_version_multiuser

---

## 相关说明

> * 在 `wine_stable_version_framework` 基础上建立的分支，并且合并分支 `wine_version_session`
> * 编译环境：虚拟机开发环境 `hotpot_1.1.0_0604`

## 编译步骤

```
（chroot 进 子系统）
cd /usr/lib/i386-linux-gnu
ln -s libgtk-3.so.0 libgtk-3.so
ln -s libgdk-3.so.0 libgdk-3.so
ln -s libpangocairo-1.0.so.0 libpangocairo-1.0.so
ln -s libpango-1.0.so.0 libpango-1.0.so
ln -s libatk-1.0.so.0 libatk-1.0.so
ln -s libcairo-gobject.so.2 libcairo-gobject.so
ln -s libcairo.so.2 libcairo.so
ln -s libgdk_pixbuf-2.0.so.0 libgdk_pixbuf-2.0.so
cd /root/winuxengine_dev/WT-Mechanism-Code
./configure
make -j5
make install
```

如果报错:

1. 选择进入 libs/port 和 libs/wine 单独编译：

  ```
cd libs/port
make
cd libs/wine
make
  ```

  其原因是 Makefile 问题，依赖没写好，以及多线程编译（-jN）问题

2. gtk/gtk.h 问题，从 `主系统` 进入 dlls/comdlg32 单独编译

  ```
cd dlls/comdlg32
make
  ```

## 最初版本（`wine_stable_version_framework`）编译过程问题

记录在 `wine_stable_version_framework` 中编译遇到的问题

### 1. ./configure 时遇到的问题

```
dlls/ntdll/../../operators/rebase/rebase.c:12: error: config.h must be included before anything else
```

解决方法修改相应文件

```
vim operators/rebase/rebase.c
```

### 2. libs/wine 问题

```
make[1]: Entering directory '/root/winuxengine_dev/WT-Mechanism-Code/container'
gcc -m32 -o wine-container-installed container.o \
  -Wl,--rpath,\$ORIGIN/`../tools/makedep -R /usr/local/bin /usr/local/lib` -Wl,--enable-new-dtags \
  ../libs/port/libwine_port.a -Wl,--export-dynamic -Wl,-Ttext-segment=0x7c000000 \
  -Wl,-z,max-page-size=0x1000 -lwine -lpthread -L../libs/wine 
container.o: In function `main':
container.c:(.text.startup+0xb2): undefined reference to `wine_init_container'
collect2: error: ld returned 1 exit status
Makefile:186: recipe for target 'wine-container-installed' failed
```

Makefile 问题，进入 `libs/wine` 编译

```
cd libs/wine
make
```

### 3. gtk+-3.0 问题

```
make[1]: Entering directory '/root/winuxengine_dev/WT-Mechanism-Code/dlls/comdlg32'
gcc -m32 -c -o filedlg.o filedlg.c -I. -I../../include -D__WINESRC__ -D_REENTRANT -fPIC -Wall -pipe \
  -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
  -Wshift-overflow=2 -Wstrict-prototypes -Wtype-limits -Wunused-but-set-parameter -Wvla \
  -Wwrite-strings -Wpointer-arith -Wlogical-op -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer \
  `pkg-config --cflags gtk+-3.0` 
Package gtk+-3.0 was not found in the pkg-config search path.
Perhaps you should add the directory containing `gtk+-3.0.pc'
to the PKG_CONFIG_PATH environment variable
No package 'gtk+-3.0' found
filedlg.c:56:21: fatal error: gtk/gtk.h: No such file or directory
 #include <gtk/gtk.h>
                     ^
compilation terminated.
Makefile:248: recipe for target 'filedlg.o' failed
make[1]: *** [filedlg.o] Error 1
```

额外功能，使用到 gtk+-3.0，`子系统` 不支持，所以进入 `主系统` 编译

```
cd dlls/comdlg32
make
```

### 4. sys/cdefs.h 缺失问题

```
gcc -m32 -c -o filedlg.o filedlg.c -I. -I../../include -D__WINESRC__ -D_REENTRANT -fPIC -Wall -pipe \
  -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
  -Wshift-overflow=2 -Wstrict-prototypes -Wtype-limits -Wunused-but-set-parameter -Wvla \
  -Wwrite-strings -Wpointer-arith -Wlogical-op -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer \
  `pkg-config --cflags gtk+-3.0` 
In file included from /usr/include/fcntl.h:25:0,
                 from ../../include/wine/port.h:35,
                 from filedlg.c:49:
/usr/include/features.h:364:25: fatal error: sys/cdefs.h: 没有那个文件或目录
 #  include <sys/cdefs.h>
                         ^
compilation terminated.
Makefile:248: recipe for target 'filedlg.o' failed
```

在 `3` 中，编译会遇到的问题，修改 Makefile.in 后，`子系统` 中修改完后进入 `主系统` 编译

```
cd dlls/comdlg32
vim Makefile.in
cd ../..
./configure
```

### 5. guint64 问题

```
gcc -m32 -c -o filedlg.o filedlg.c -I. -I../../include -D__WINESRC__ -D_REENTRANT -fPIC -Wall -pipe \
  -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
  -Wshift-overflow=2 -Wstrict-prototypes -Wtype-limits -Wunused-but-set-parameter -Wvla \
  -Wwrite-strings -Wpointer-arith -Wlogical-op -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer \
  `pkg-config --cflags gtk+-3.0` -I../../../../../usr/include/i386-linux-gnu/ 
In file included from /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h:9:0,
                 from /usr/include/glib-2.0/glib/gtypes.h:32,
                 from /usr/include/glib-2.0/glib/galloca.h:32,
                 from /usr/include/glib-2.0/glib.h:30,
                 from /usr/include/gtk-3.0/gdk/gdkconfig.h:13,
                 from /usr/include/gtk-3.0/gdk/gdk.h:30,
                 from /usr/include/gtk-3.0/gtk/gtk.h:30,
                 from filedlg.c:56:
/usr/include/glib-2.0/glib/gtypes.h: In function ‘_GLIB_CHECKED_ADD_U64’:
/usr/include/glib-2.0/glib/gmacros.h:232:53: error: size of array ‘_GStaticAssertCompileTimeAssertion_0’ is negative
 #define G_STATIC_ASSERT(expr) typedef char G_PASTE (_GStaticAssertCompileTimeAssertion_, __COUNTER__)[(expr) ? 1 : -1] G_GNUC_UNUSED
                                                     ^
/usr/include/glib-2.0/glib/gmacros.h:229:47: note: in definition of macro ‘G_PASTE_ARGS’
 #define G_PASTE_ARGS(identifier1,identifier2) identifier1 ## identifier2
                                               ^~~~~~~~~~~
/usr/include/glib-2.0/glib/gmacros.h:232:44: note: in expansion of macro ‘G_PASTE’
 #define G_STATIC_ASSERT(expr) typedef char G_PASTE (_GStaticAssertCompileTimeAssertion_, __COUNTER__)[(expr) ? 1 : -1] G_GNUC_UNUSED
                                            ^~~~~~~
/usr/include/glib-2.0/glib/gtypes.h:423:3: note: in expansion of macro ‘G_STATIC_ASSERT’
   G_STATIC_ASSERT(sizeof (unsigned long long) == sizeof (guint64));
   ^~~~~~~~~~~~~~~
Makefile:249: recipe for target 'filedlg.o' failed
```

这个问题是 `glibconfig.h` 中的 `guint64` `typedef` 有问题，应该是 `unsigned long long`，在 `i386` 中是这样的，但在 `x86_64` 中是 `unsigned long`，所以直接使用 `i386` 中的，也就是 `子系统` 的，修改 `Makefile.in` 做到

```
cd dlls/comdlg32
vim Makefile.in
cd ../..
./configure
```

以上` 3、4、5` 是使用了 `pkg-config --cflags gtk+-3.0`，在 `子系统` 中不支持

### 6. ld 问题

```
../../tools/winegcc/winegcc -o comdlg32.dll.so -B../../tools/winebuild -m32 -fasynchronous-unwind-tables -shared comdlg32.spec \
  cdlg32.o colordlg.o filedlg.o filedlg31.o filedlgbrowser.o finddlg.o fontdlg.o itemdlg.o \
  printdlg.o comdlg32.res comdlg32_classes_r.res -lole32 ../../dlls/uuid/libuuid.a -lshell32 \
  -lshlwapi -lcomctl32 -lwinspool -luser32 -lgdi32 -ladvapi32 -lgtk-3 -lgdk-3 -lpangocairo-1.0 \
  -lpango-1.0 -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 \
  ../../libs/port/libwine_port.a -Wb,-dole32 
/usr/bin/ld: 找不到 crti.o: 没有那个文件或目录
/usr/bin/ld: 找不到 -lgtk-3
/usr/bin/ld: 找不到 -lgdk-3
/usr/bin/ld: 找不到 -lpangocairo-1.0
/usr/bin/ld: 找不到 -lpango-1.0
/usr/bin/ld: 找不到 -latk-1.0
/usr/bin/ld: 找不到 -lcairo-gobject
/usr/bin/ld: 找不到 -lcairo
/usr/bin/ld: 找不到 -lgdk_pixbuf-2.0
/usr/bin/ld: 找不到 -lgio-2.0
/usr/bin/ld: 找不到 -lgobject-2.0
/usr/bin/ld: 找不到 -lglib-2.0
/usr/bin/ld: 找不到 -lm
/usr/bin/ld: 找不到 -lc
/usr/bin/ld: 当搜索用于 /usr/lib/gcc/x86_64-linux-gnu/6/libgcc.a 时跳过不兼容的 -lgcc 
/usr/bin/ld: 找不到 -lgcc
/usr/bin/ld: 当搜索用于 /usr/lib/gcc/x86_64-linux-gnu/6/libgcc.a 时跳过不兼容的 -lgcc 
/usr/bin/ld: 找不到 -lgcc
collect2: error: ld returned 1 exit status
winegcc: gcc failed
Makefile:508: recipe for target 'comdlg32.dll.so' failed
```

上面是在 `主系统` `dlls/comdlg32` 中编译会遇到的问题，回到 `子系统` 中编译，会少掉一部分错误，进行如下处理之后，可以编译通过：

```
cd /usr/lib/i386-linux-gnu
ln -s libgtk-3.so.0 libgtk-3.so
ln -s libgdk-3.so.0 libgdk-3.so
ln -s libpangocairo-1.0.so.0 libpangocairo-1.0.so
ln -s libpango-1.0.so.0 libpango-1.0.so
ln -s libatk-1.0.so.0 libatk-1.0.so
ln -s libcairo-gobject.so.2 libcairo-gobject.so
ln -s libcairo.so.2 libcairo.so
ln -s libgdk_pixbuf-2.0.so.0 libgdk_pixbuf-2.0.so
```

### 7. 代码错误

```
../../operators/rebase/rebase.c: At top level:
../../operators/rebase/rebase.c:255:1: error: expected identifier or '(' before '}' token
 }
 ^
Makefile:782: recipe for target '../../operators/rebase/rebase.o' failed
```

不知道怎么多出来一个 `}`。删去就可以了

```
vim operators/rebase/rebase.c
```


## `wine_version_session` 编译过程问题

### 1. libs/port 问题

```
gcc -m32 -c -o version.o version.c -I. -I../../include -D__WINESRC__ -DWINE_UNICODE_API="" -D_REENTRANT -fPIC \
  -Wall -pipe -fno-strict-aliasing -Wdeclaration-after-statement -Wempty-body -Wignored-qualifiers \
  -Wshift-overflow=2 -Wstrict-prototypes -Wtype-limits -Wunused-but-set-parameter -Wvla \
  -Wwrite-strings -Wpointer-arith -Wlogical-op -gdwarf-2 -gstrict-dwarf -fno-omit-frame-pointer \
  -g -O2
make: *** No rule to make target '../../libs/port/libwine_port.a', needed by 'libwine.so.1.0'.  Stop.\
```

cd libs/wine 后出现的问题

```
cd libs/port
make
```
