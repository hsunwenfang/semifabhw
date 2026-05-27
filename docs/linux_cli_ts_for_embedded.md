
# Linux Concepts

## Paths and Mount Backing

- `/proc`, `/sys`: Virtual fs allocated by kernel to RAM at runtime

- `/proc`
    Broad runtime kernel (text name) + userspace process (process id) information.
    Includes per-process dirs (/proc/<pid>) and global runtime files (/proc/meminfo, /proc/interrupts).

- `/proc/sys`
    Writable/readable kernel tunables (sysctl interface).
    This is the runtime config tree for `kernel parameters`.
    /proc/sys/net/ipv4/ip_forward | /proc/sys/vm/swappiness

- `/sys`
    Sysfs: kernel device model and driver object hierarchy.
    `/sys/devices` holds real info
    `/sys/buses`, `/sys/classes` are softlinks giving connection view, function views
    /sys/class/net/eth0 | /sys/bus/i2c/devices | /sys/class/pwm/pwmchip0

- `/dev`: Device node namespace. `devtmpfs` in RAM
    - `/lib/systemd/systemd-udevd` process is in charge

- `/run`: Volatile runtime state for current boot. Usually `tmpfs` in RAM.
- `/tmp`: Temporary app workspace. Often `tmpfs` on embedded, but may also be on root filesystem.
- `/proc/sys` kernel tunables
    - cat /proc/sys/net/ipv6/ip_nonlocal_bind -> 0
- `mount | grep -E " on /(proc|sys|dev|run|tmp| )"`

- check device id mapping
```sh
ls -l /dev/disk
total 0
drwxr-xr-x 3 root root 140 May 27 05:03 azure
drwxr-xr-x 2 root root 340 May 27 05:03 by-id
drwxr-xr-x 2 root root  80 May 27 05:03 by-label
drwxr-xr-x 2 root root 100 May 27 05:03 by-partuuid
drwxr-xr-x 2 root root 140 May 27 05:03 by-path
drwxr-xr-x 2 root root  80 May 27 05:03 by-uuid
###
/dev/disk/by-uuid/85a32921-52b7-4d2d-9c02-a565b70bc919 -> ../../nvme0n1p1
```

### Relevant TS Guide

#### TS 2: Root filesystem mounted read-only unexpectedly

    - Confirm current mount flags and check configured mount policy to find why writes are blocked.
    - `mount`
        - `source` on `target` type `fstype` (options)
        - /dev/nvme0n1p1 on / type ext4 (rw,relatime,discard,errors=remount-ro)
        - devtmpfs on /dev type devtmpfs (rw,nosuid,noexec,relatime,size=16428884k,nr_inodes=4107221,mode=755,inode64)
    - `/proc/mounts`
    - `/etc/fstab` : filesystem table -> what to mount right after boot
        - UUID=85a32921-52b7-4d2d-9c02-a565b70bc919 / ext4   discard,errors=remount-ro  0 1
        - uuid to device name

#### TS 9: PWM output absent on pin

    - Export channel and verify period/duty/enable sequencing to ensure PWM (Pulse Width Modulation) is actually active.
    `cat /sys/class/pwm/pwmchip0/export`
    `echo 0 > /sys/class/pwm/pwmchip0/export` export control file so pwmchip0 can be controlled
    - `timing` `pin configuration` `state management`

#### TS 22: Flash storage almost full

    - Measure filesystem and directory usage to prioitize cleanup or log-rotation fixes.
    - mount or findmnt: what is mounted, where, and with which type/options
    df -h: how much real filesystem space is used/available
    - FS path(s): `/var/`, `/tmp/`, `/home/`
    - Linux concept: Capacity and disk usage

#### TS 23: eMMC/SD card I/O errors appear

    - Description: Inspect kernel storage errors and health indicators to assess media degradation.
    - Linux command(s): `dmesg | grep -i -E "mmc|I/O error"`, `smartctl -a /dev/mmcblk0`
    - FS path(s): `/dev/mmcblk*`, `/sys/block/mmcblk0/`
    - Linux concept: Block device health

#### TS 24: Read/write performance is too low

    - Description: Collect latency and throughput metrics to distinguish queueing, media, and workload bottlenecks.
    - Linux command(s): `iostat -x 1`, `fio --name=randrw ...`
    - FS path(s): `/sys/block/`, `/proc/diskstats`
    - Linux concept: I/O throughput and latency

## Systemd Namespaces and Units

- `systemd` as `PID 1` manages units (`.service`, `.socket`, `.mount`, `.target`, and others).
- Strongest control is through `cgroup v2` for process lifecycle and resource control.
- Namespace behavior:
  - `systemd` can apply per-unit namespace isolation (mount, IPC, PID, network, user, UTS).
  - It does not globally "own" namespaces; kernel provides namespaces, systemd configures usage per unit.

- Useful checks:
  - `ps -p 1 -o pid,comm,args`
  - `systemctl status`
  - `systemd-cgls`
  - `systemctl show <unit> | grep -E "Namespace|Private|Protect"`
  - `lsns`

### Relevant TS Guide

#### TS 15: Process crashes intermittently
#### TS 16: High CPU usage causes missed control loop deadlines
#### TS 19: Service does not start at boot
#### TS 20: Need startup order dependency fix

## Boot Porcess

1. Boot ROM -> bootloader
    - Hardware reset path enters ROM, then first/second stage loader.
2. Bootloader phase
    - Bootloader (for example U-Boot) initializes minimum hardware, loads `kernel`, `DTB`, and optional `initramfs`.
    - `DTB` (Device Tree Blob) describes board hardware (CPU, RAM, buses, IRQ, devices).
    - `initramfs` is an early userspace filesystem in RAM for preparation tasks.
    - Bootloader sets bootargs and jumps to kernel entry.
3. Init phase (kernel early init)
    - Kernel initializes core subsystems: process/task model, virtual memory manager, interrupt handling, scheduler, VFS, and driver model.
    - `flash` and `AHB` are hardware/platform components; kernel accesses them through controllers and drivers.
4. Driver probe phase
    - Kernel buses enumerate devices and run probe functions to bind matching drivers.
5. Mount phase
    - Kernel mounts real root filesystem, then runtime mounts such as `/proc`, `/sys`, `/dev`, `/run`, `/tmp`, and optionally `/boot`, `/var`, `/home`, `/data`.
    - Common issues: wrong `root=`/`rootfstype=`, bad `fstab` options, missing device, read-only fallback.
6. Userspace handoff phase
    - Kernel executes `PID 1` (`systemd`/`init`/`busybox init`) and userspace startup begins.
    - Services are managed by `systemctl` only when `systemd` is PID 1; applications may also be launched by scripts or other supervisors.

flash/disk stores program image
boot or exec loads needed parts into RAM
CPU executes from RAM, not directly from regular filesystem files

- Does handoff mean PC pointer change?
  - Yes. Control flow jumps to the next entry point (program counter changes) and includes full context setup (memory mappings, arguments, privilege/domain context).

- Useful checks by stage:
  - Boot args: `cat /proc/cmdline`
  - Kernel logs: `dmesg -T | less`
  - Live kernel logs: `dmesg -w`
  - PID 1 identity: `cat /proc/1/comm`
  - Early userspace status: `systemctl --failed`

### Relevant TS Guide

#### TS 1: Device does not boot past kernel logo

Check kernel ring buffer to identify where boot stops (bootloader handoff, init, probe, mount, or userspace start).

```sh
dmesg -T
cat /proc/cmdline # shows the boot argument while 
cat /var/log # may be empty if userspace passing is incomplete
```

#### TS 3: Application binary fails with "No such file or directory"

Verify binary architecture and dynamic linker dependencies
this error often indicates missing loader/lib, not missing file.

```sh
g++ test.cpp -o test
g++ test.cpp -static -o test_static

 file test
test: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=4c9940ac6379f4bcb95cb62f9cdd04d077117316, for GNU/Linux 3.2.0, not stripped

file test_static
test_static: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), statically linked, BuildID[sha1]=83d4d3437f2e4c25a3b8e1535e4465b3cf8e4e13, for GNU/Linux 3.2.0, not stripped

ldd test
linux-vdso.so.1 (0x00007ffc58d16000)
libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x0000744cc2a00000)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x0000744cc2600000)
libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x0000744cc2d30000)
/lib64/ld-linux-x86-64.so.2 (0x0000744cc2e24000)
libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x0000744cc2d10000)

```

#### TS 5: USB sensor not detected
#### TS 6: I2C peripheral not responding
#### TS 7: SPI device communication mismatch
#### TS 10: Network interface is down after boot
#### TS 11: Embedded board cannot get IP via DHCP
#### TS 12: DNS resolution fails but ping by IP works
#### TS 13: NTP time sync not working
#### TS 14: RTC time drifts after power cycle
#### TS 18: Out-of-memory kills critical process
#### TS 25: Kernel module not loaded for peripheral
#### TS 26: Device-tree change not taking effect

```sh
cat /proc/cmdline
BOOT_IMAGE=/boot/vmlinuz-6.8.0-1052-azure root=PARTUUID=fb505a37-7427-4a67-b014-dcfeb6c00245 ro console=tty1 console=ttyS0 earlyprintk=ttyS0 nvme_core.io_timeout=240 panic=-1
```

## User management, permission, policy, linuxpriviledges, sudo

- Subject `uid`, `gid`, `process`
- Object `device`, `inode`
- DAC Discretionary Access Contorl for file
    uid, gid and supplementary group 
    mode bits: dr-xr-xr-x == [type][owner user][owner group][other]
    ACL extends this by file `getfacl`
    setuid/setgid turns process credential from caller uid/gid to owner uid/gid
    sticky bits
        sticky: on shared directories, only file owner/root can delete or rename entries
- Linux capabilities `grep Cap /proc/$$/status`
    getcap: capabilities attached to executable files (xattr security.capability)
    /proc/<pid>/status (CapEff, CapPrm, CapInh, etc): capabilities currently held by a process
    capsh --print: process capability sets in current shell context
    CAP_NET_ADMIN | CAP_SYS_ADMIN can only assign to process
- MAC permissions : SELinux, AppArmor
    policy-based access control
    SELinux: `getenforce`, `sestatus`, `ls -Z <path>`, `ps -eZ`
    AppArmor: `aa-status`, `cat /sys/module/apparmor/parameters/enabled`, `journalctl -k | grep -i apparmor`
- Mount options
    `ro` blocks writes even if file permissions would allow writing.
    `noexec` blocks executing binaries from that mount even if x bit is set.
- Privilege escalation
    - sudo / polkit
    - `/etc/sudoers`, `/etc/sudoers.d/`.
    - check polkit:
        `pkaction`
- Device policy: `udev` assigns ownership/mode/group on device nodes under `/dev`.
    this is DAC permission setup for device-node objects (uid/gid/mode bits, and optionally ACL via udev rules).

- `id`
    uid=1000(hsunwen) gid=1000(hsunwen) groups=1000(hsunwen),4(adm),20(dialout),24(cdrom),25(floppy),27(sudo),29(audio),30(dip),44(video),46(plugdev),119(netdev),120(lxd)
- `groups <user>`
- `ls -l /dev/<node>`
- `getcap -r / 2>/dev/null | head`

### Relevant TS Guide

#### TS 27: Permission denied when accessing device node
        - Description: Check owner/group/mode and ACLs, then confirm udev rules produce intended permissions.
        - Linux command(s): `ls -l /dev/<node>`, `id`, `getfacl /dev/<node>`
        - FS path(s): `/dev/`, `/etc/udev/rules.d/`
        - Linux concept: Unix permissions and udev

#### TS 28: Hotplug event rule not executing
        - Description: Validate udev rule matching and execution path for add/remove events.
        - Linux command(s): `udevadm monitor`, `udevadm test /sys/class/...`
        - FS path(s): `/etc/udev/rules.d/`, `/run/udev/`
        - Linux concept: udev policy and device event pipeline



## Process

`ps -p 194 -o pid,ppid,user,comm,args`



# Linux CLI TS Scenarios for Embedded Systems

TS = troubleshooting scenario. This list maps common embedded Linux scenarios to the command, filesystem location, and core concept you need.


## 2) Root filesystem mounted read-only unexpectedly

## 3) Application binary fails with "No such file or directory"

## 4) Serial port not receiving data

- Description: Validate baud, parity, stop bits, and raw/canonical mode before assuming hardware failure.
- Linux command(s): `stty -F /dev/ttyS0 -a`, `cat /dev/ttyS0`
- FS path(s): `/dev/ttyS*`, `/sys/class/tty/`
- Linux concept: TTY configuration

## 5) USB sensor not detected

- Description: Check bus enumeration and kernel probe messages to confirm whether the device is electrically visible and driver-bound.
- Linux command(s): `lsusb`, `dmesg | grep -i usb`
- FS path(s): `/sys/bus/usb/devices/`
- Linux concept: USB enumeration

## 6) I2C peripheral not responding

- Description: Probe the bus address map and attempt register reads to isolate wiring, addressing, or power issues.
- Linux command(s): `i2cdetect -y 1`, `i2cget -y 1 0x48`
- FS path(s): `/dev/i2c-1`, `/sys/bus/i2c/devices/`
- Linux concept: I2C bus probing

## 7) SPI device communication mismatch

- Description: Verify SPI node existence and inspect raw traffic behavior for mode/clock/chip-select mismatch clues.
- Linux command(s): `ls /dev/spidev*`, `hexdump -C /dev/spidev0.0`
- FS path(s): `/dev/spidev*`, `/sys/bus/spi/devices/`
- Linux concept: SPI node and transfer validation

## 8) GPIO line cannot be toggled

- Description: Identify line ownership and direction; contention by another driver/process is a common cause.
- Linux command(s): `gpioinfo`, `gpioset gpiochip0 17=1`
- FS path(s): `/dev/gpiochip*`, `/sys/kernel/debug/gpio`
- Linux concept: GPIO ownership and line state

## 9) PWM output absent on pin

## 10) Network interface is down after boot

- Description: Check link state and manually bring interface up to differentiate config issue from driver or PHY issue.
- Linux command(s): `ip link show`, `ip link set eth0 up`
- FS path(s): `/sys/class/net/`, `/etc/network/`
- Linux concept: Link state management

## 11) Embedded board cannot get IP via DHCP

- Description: Run DHCP client in verbose mode to inspect discover/offer/request/ack flow and lease failures.
- Linux command(s): `udhcpc -i eth0 -v`, `ip addr`
- FS path(s): `/etc/resolv.conf`, `/var/lib/`
- Linux concept: DHCP client behavior

## 12) DNS resolution fails but ping by IP works

- Description: Validate resolver configuration and query path to separate network reachability from name service problems.
- Linux command(s): `cat /etc/resolv.conf`, `nslookup example.com`
- FS path(s): `/etc/resolv.conf`
- Linux concept: Name resolution chain

## 13) NTP time sync not working

- Description: Confirm local time service state and peer synchronization to prevent timestamp and TLS issues.
- Linux command(s): `timedatectl status`, `ntpq -p`
- FS path(s): `/etc/systemd/timesyncd.conf`, `/var/lib/systemd/timesync/`
- Linux concept: Time synchronization

## 14) RTC time drifts after power cycle

- Description: Compare hardware clock and system clock behavior across reboot to find backup-power or sync process issues.
- Linux command(s): `hwclock -r`, `hwclock -w`
- FS path(s): `/dev/rtc0`, `/sys/class/rtc/rtc0/`
- Linux concept: RTC vs system clock

## 15) Process crashes intermittently

- Description: Correlate service logs with core dump records to identify recurring crash signatures.
- Linux command(s): `journalctl -u app.service -n 200`, `coredumpctl list`
- FS path(s): `/var/log/journal/`, `/var/lib/systemd/coredump/`
- Linux concept: Service logs and core dumps

## 16) High CPU usage causes missed control loop deadlines

- Description: Find top CPU consumers and scheduling priorities to diagnose realtime jitter and starvation.
- Linux command(s): `top`, `ps -eo pid,comm,rtprio,ni,%cpu --sort=-%cpu`
- FS path(s): `/proc/`, `/sys/fs/cgroup/`
- Linux concept: Scheduling and CPU profiling

## 17) Memory leak suspected in long run

- Description: Track resident memory trends and process mappings over time to confirm progressive growth.
- Linux command(s): `free -m`, `cat /proc/<pid>/status`
- FS path(s): `/proc/<pid>/smaps`, `/proc/meminfo`
- Linux concept: Memory accounting

## 18) Out-of-memory kills critical process

- Description: Read kernel OOM events to identify victim process and tune memory behavior accordingly.
- Linux command(s): `dmesg | grep -i -E "oom|killed process"`
- FS path(s): `/proc/sys/vm/`, `/var/log/`
- Linux concept: OOM killer behavior

## 19) Service does not start at boot

- Description: Verify unit status and enablement state to catch missing symlinks, bad dependencies, or runtime failures.
- Linux command(s): `systemctl status app.service`, `systemctl is-enabled app.service`
- FS path(s): `/etc/systemd/system/`, `/lib/systemd/system/`
- Linux concept: systemd unit lifecycle

## 20) Need startup order dependency fix

- Description: Inspect merged unit files and dependency tree to enforce correct sequencing.
- Linux command(s): `systemctl cat app.service`, `systemctl list-dependencies`
- FS path(s): `/etc/systemd/system/*.service`
- Linux concept: Unit dependency graph

## 21) Firmware update package checksum mismatch

- Description: Verify downloaded artifact integrity before flashing to prevent bricking or partial updates.
- Linux command(s): `sha256sum firmware.bin`
- FS path(s): `/tmp/`, `/opt/firmware/`
- Linux concept: Integrity verification

## 22) Flash storage almost full

## 23) eMMC/SD card I/O errors appear

## 24) Read/write performance is too low

## 25) Kernel module not loaded for peripheral

- Description: Check whether the module exists, is loaded, and exposes expected metadata for autoloading.
- Linux command(s): `lsmod`, `modprobe <module>`, `modinfo <module>`
- FS path(s): `/lib/modules/$(uname -r)/`, `/etc/modules-load.d/`
- Linux concept: Module management

## 26) Device-tree change not taking effect

- Description: Verify active DT blob content and boot logs to confirm your updated DTB/overlay is actually used.
- Linux command(s): `hexdump -C /sys/firmware/fdt`, `dmesg | grep -i dt`
- FS path(s): `/boot/`, `/sys/firmware/devicetree/base/`
- Linux concept: DTB loading and overlays

## 27) Permission denied when accessing device node

## 28) Hotplug event rule not executing

- Description: Observe live udev events and test rules to find match condition or action failures.
- Linux command(s): `udevadm monitor`, `udevadm test /sys/class/...`
- FS path(s): `/etc/udev/rules.d/`, `/run/udev/`
- Linux concept: udev event pipeline

## 29) Need to inspect live kernel tunables

- Description: Read runtime sysctl values and compare them with persisted config to verify applied tuning.
- Linux command(s): `sysctl -a | grep <key>`, `cat /proc/sys/...`
- FS path(s): `/proc/sys/`, `/etc/sysctl.conf`, `/etc/sysctl.d/`
- Linux concept: Runtime kernel parameters

## 30) Suspect network packet loss in field

- Description: Combine ICMP tests, NIC counters, and packet capture to localize loss on host, link, or upstream path.
- Linux command(s): `ping -c 50 <ip>`, `ethtool -S eth0`, `tcpdump -i eth0`
- FS path(s): `/sys/class/net/eth0/statistics/`
- Linux concept: Link diagnostics and packet capture

## Quick Usage Notes

- On minimal embedded distributions, some tools may not be installed by default (for example: `iostat`, `fio`, `ethtool`, `tcpdump`).
- If `systemd` is not used, replace `systemctl`/`journalctl` with init scripts and `/var/log/messages` style logs.
- Prefer read-only inspection first (`cat`, `ls`, `dmesg`) before running write operations on production hardware.
