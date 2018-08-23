# 测试

---

## 容器分离

- `dev` 用户下直接运行 `wine notepad`

```
dev@dev:~$ wine notepad
connect: No such file or directory
connect: No such file or directory
wine: wine-container not started: chdir to /tmp/.wine/server-805-2438ab : No such file or directory
```

- `root` 下运行 `wine-container`

```
root@dev:~# wine-container 
wine-container: start....
wineserver receive msg:from winemenubuilder,pid is 2727
winemenubuilder receive msg:from wineserver,pid is 2727
pnp-device-arrival
pnp-device-arrival
pnp-device-arrival
err:winediag:nulldrv_CreateWindow Application tried to create a window, but no driver could be loaded.
err:winediag:nulldrv_CreateWindow The explorer process failed to start.
wine-container pre execute:L"/opt/.wine/drive_c/windows/system32/winemenubuilderservice.exe"
err:winediag:nulldrv_CreateWindow Application tried to create a window, but no driver could be loaded.
err:winediag:nulldrv_CreateWindow The explorer process failed to start.
err:hid:PNP_AddDevice Cannot get Device Descriptor(c0000001)
err:plugplay:handle_bus_relations AddDevice failed for driver L"WineHID"
wine-container:start over, enjoy your work!
```

- `dev` 用户下在 `wine-container` 运行之后再运行 `wine notepad`

```
dev@dev:~$ wine notepad
connect: Permission denied
connect: No such file or directory
```

## 多用户测试

### 1. 用户标识

正确

```
dev@dev:~$ ps -aux
root      2687  0.0  0.3   9528  6484 ?        Ss   14:58   0:00 /var/lib/mock/cdos-2.2.0-build-i386/root/usr/local/bin/wineserver
root      2693  0.0  0.4 2655248 9452 ?        Ssl  14:58   0:00 C:\windows\system32\services.exe
root      2699  0.0  0.5 2658228 10512 ?       Sl   14:58   0:00 C:\windows\system32\winedevice.exe
root      2711  0.0  0.4 2653828 9088 ?        Sl   14:58   0:00 C:\windows\system32\plugplay.exe
root      2717  0.0  0.5 2656336 10544 ?       Sl   14:58   0:00 C:\windows\system32\winedevice.exe
root      2727  1.0  0.5 2675812 11408 ?       Sl   14:58   0:06 C:\windows\system32\winemenubuilderservice.exe
dev       2958  0.3  1.2 2710172 26304 pts/2   S+   15:09   0:00 notepad
dev       2961  0.6  0.7 2679160 15316 ?       Ssl  15:09   0:00 C:\windows\system32\explorer.exe /desktop

```

### 2. 用户权限

1. 文件创建用户错误：`dev` 用户创建文件 `own` 和 `group` 都是 `root`
2. 以 `dev` 身份运行 `notepad filename` （或者 `wine notepad filename`）
  - 如果 `filename` 是 `root` 的，可以修改保存。
  - 如果 `filename` 是 `dev` 的，那么保存的文件还是 `dev`，但是 `另存为` 文件是 `root`

> * ~~推测：实际用户标识还是 `root`，只是 `ps -aux` 上成为了相应用户~~
> * 主体多用户实现，主体对客体的操作还有问题

### 3. 用户管理

没找到如何显示用户，创建用户方法，尝试过以下方法

> * cmd -> whoami
> * cmd -> net user

以上命令均无法使用

- [ ] 从注册表角度分析

## 运行中的问题：

### 1. 启动 wine-container 界面的 debug 输出

```
...
debug by danqi != string1 91000
debug by danqi != int 91000
debug by danqi != local_user_sid 91000
debug by danqi security_unix_uid_to_sid uid!=0
debug by danqi != string1 91000
debug by danqi != int 91000
debug by danqi != local_user_sid 91000
debug by danqi security_unix_uid_to_sid uid!=0
...
```

运行程序会打印这些信息，不知道什么意思

### 2. 中文输入问题（以 notepad 为例）

输入中文

```
fixme:imm:ImmReleaseContext (0xc0052, 0x1d5080): stub
```
