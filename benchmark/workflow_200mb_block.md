### fio 워크플로우 (200MB 크기의 블록 디바이스 파일시스템)

1. **순차 쓰기 → 순차 읽기**

   ```bash
   # 1) 순차 쓰기
   fio --name=seq-write \
       --filename=/mnt/partition_04_200MB/fiotest.dat \
       --size=100M --bs=1M --rw=write --direct=1 --ioengine=sync --iodepth=1

   # 2) 캐시 비우기
   echo 3 | sudo tee /proc/sys/vm/drop_caches

   # 3) 순차 읽기 (같은 파일)
   fio --name=seq-read \
       --filename=/mnt/partition_04_200MB/fiotest.dat \
       --size=100M --bs=1M --rw=read --direct=1 --ioengine=sync --iodepth=1
   ```

2. **랜덤 쓰기 → 랜덤 읽기**

   ```bash
   # 1) 랜덤 쓰기
   fio --name=rand-write \
       --filename=/mnt/partition_04_200MB/fiotest-rand.dat \
       --size=100M --bs=4k --rw=randwrite --direct=1 --ioengine=libaio --iodepth=16

   # 2) 캐시 비우기
   echo 3 | sudo tee /proc/sys/vm/drop_caches

   # 3) 랜덤 읽기 (같은 파일)
   fio --name=rand-read \
       --filename=/mnt/partition_04_200MB/fiotest-rand.dat \
       --size=100M --bs=4k --rw=randread --direct=1 --ioengine=libaio --iodepth=16
   ```
   
---
### SFUSE의 워크플로우 결과 (200MB 크기의 블록 디바이스 파일시스템)
1. **SFUSE의 "순차 쓰기 → 순차 읽기"**
```bash
root@thinkpad:/mnt/partition_04_200MB # 
fio --name=seq-write \
    --filename=/mnt/partition_04_200MB/fiotest.dat \
    --size=100M --bs=1M --rw=write --direct=1 --ioengine=sync --iodepth=1
seq-write: (g=0): rw=write, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process
seq-write: Laying out IO file (1 file / 100MiB)
Jobs: 1 (f=1): [W(1)][100.0%][w=7175KiB/s][w=7 IOPS][eta 00m:00s]
seq-write: (groupid=0, jobs=1): err= 0: pid=127960: Thu Jul 10 15:20:18 2025
  write: IOPS=7, BW=7876KiB/s (8065kB/s)(100MiB/13001msec); 0 zone resets
    clat (msec): min=113, max=145, avg=129.89, stdev= 8.33
     lat (msec): min=113, max=145, avg=129.99, stdev= 8.34
    clat percentiles (msec):
     |  1.00th=[  114],  5.00th=[  117], 10.00th=[  118], 20.00th=[  122],
     | 30.00th=[  125], 40.00th=[  128], 50.00th=[  130], 60.00th=[  133],
     | 70.00th=[  136], 80.00th=[  138], 90.00th=[  142], 95.00th=[  142],
     | 99.00th=[  146], 99.50th=[  146], 99.90th=[  146], 99.95th=[  146],
     | 99.99th=[  146]
   bw (  KiB/s): min= 6144, max=10240, per=99.84%, avg=7864.32, stdev=967.85, samples=25
   iops        : min=    6, max=   10, avg= 7.68, stdev= 0.95, samples=25
  lat (msec)   : 250=100.00%
  cpu          : usr=0.10%, sys=0.08%, ctx=202, majf=0, minf=6
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,100,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
  WRITE: bw=7876KiB/s (8065kB/s), 7876KiB/s-7876KiB/s (8065kB/s-8065kB/s), io=100MiB (105MB), run=13001-13001msec

```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```

```bash
root@thinkpad:/mnt/partition_04_200MB # 
fio --name=seq-read \
    --filename=/mnt/partition_04_200MB/fiotest.dat \
    --size=100M --bs=1M --rw=read --direct=1 --ioengine=sync --iodepth=1
seq-read: (g=0): rw=read, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process

seq-read: (groupid=0, jobs=1): err= 0: pid=130062: Thu Jul 10 15:21:02 2025
  read: IOPS=2325, BW=2326MiB/s (2439MB/s)(100MiB/43msec)
    clat (usec): min=199, max=2187, avg=406.39, stdev=258.82
     lat (usec): min=199, max=2188, avg=406.56, stdev=259.01
    clat percentiles (usec):
     |  1.00th=[  200],  5.00th=[  200], 10.00th=[  202], 20.00th=[  202],
     | 30.00th=[  217], 40.00th=[  281], 50.00th=[  412], 60.00th=[  437],
     | 70.00th=[  478], 80.00th=[  529], 90.00th=[  603], 95.00th=[  652],
     | 99.00th=[ 1418], 99.50th=[ 2180], 99.90th=[ 2180], 99.95th=[ 2180],
     | 99.99th=[ 2180]
  lat (usec)   : 250=35.00%, 500=41.00%, 750=21.00%, 1000=1.00%
  lat (msec)   : 2=1.00%, 4=1.00%
  cpu          : usr=0.00%, sys=9.52%, ctx=103, majf=1, minf=264
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=100,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
   READ: bw=2326MiB/s (2439MB/s), 2326MiB/s-2326MiB/s (2439MB/s-2439MB/s), io=100MiB (105MB), run=43-43msec
``` 

2. **SFUSE의 "랜덤 쓰기 → 랜덤 읽기"**
```bash
root@thinkpad:/mnt/partition_04_200MB # 
fio --name=rand-write \
    --filename=/mnt/partition_04_200MB/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randwrite --direct=1 --ioengine=libaio --iodepth=16
rand-write: (g=0): rw=randwrite, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process
rand-write: Laying out IO file (1 file / 100MiB)
Jobs: 1 (f=1): [w(1)][100.0%][w=3596KiB/s][w=899 IOPS][eta 00m:00s]
rand-write: (groupid=0, jobs=1): err= 0: pid=206693: Thu Jul 10 15:55:57 2025
  write: IOPS=945, BW=3780KiB/s (3871kB/s)(100MiB/27089msec); 0 zone resets
    slat (usec): min=41, max=6001, avg=1051.02, stdev=134.54
    clat (usec): min=974, max=22392, avg=15869.58, stdev=1073.38
     lat (usec): min=2023, max=23837, avg=16920.59, stdev=1127.02
    clat percentiles (usec):
     |  1.00th=[14091],  5.00th=[14484], 10.00th=[14746], 20.00th=[15008],
     | 30.00th=[15270], 40.00th=[15533], 50.00th=[15664], 60.00th=[16057],
     | 70.00th=[16319], 80.00th=[16712], 90.00th=[17171], 95.00th=[17695],
     | 99.00th=[19006], 99.50th=[19530], 99.90th=[21103], 99.95th=[21627],
     | 99.99th=[21890]
   bw (  KiB/s): min= 3552, max= 4048, per=100.00%, avg=3780.30, stdev=120.54, samples=54
   iops        : min=  888, max= 1012, avg=945.07, stdev=30.14, samples=54
  lat (usec)   : 1000=0.01%
  lat (msec)   : 4=0.01%, 10=0.02%, 20=99.57%, 50=0.39%
  cpu          : usr=0.82%, sys=2.44%, ctx=25825, majf=0, minf=9
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,25600,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
  WRITE: bw=3780KiB/s (3871kB/s), 3780KiB/s-3780KiB/s (3871kB/s-3871kB/s), io=100MiB (105MB), run=27089-27089msec
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```

```bash
root@thinkpad:/mnt/partition_04_200MB # fio --name=rand-read \
    --filename=/mnt/partition_04_200MB/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randread --direct=1 --ioengine=libaio --iodepth=16
rand-read: (g=0): rw=randread, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process

rand-read: (groupid=0, jobs=1): err= 0: pid=208285: Thu Jul 10 15:56:14 2025
  read: IOPS=121k, BW=472MiB/s (495MB/s)(100MiB/212msec)
    slat (nsec): min=631, max=927151, avg=7289.00, stdev=6708.15
    clat (usec): min=60, max=1260, avg=124.02, stdev=53.54
     lat (usec): min=63, max=1278, avg=131.31, stdev=56.50
    clat percentiles (usec):
     |  1.00th=[  103],  5.00th=[  106], 10.00th=[  108], 20.00th=[  110],
     | 30.00th=[  111], 40.00th=[  112], 50.00th=[  113], 60.00th=[  114],
     | 70.00th=[  116], 80.00th=[  117], 90.00th=[  122], 95.00th=[  204],
     | 99.00th=[  388], 99.50th=[  400], 99.90th=[  490], 99.95th=[ 1045],
     | 99.99th=[ 1237]
  lat (usec)   : 100=0.47%, 250=95.09%, 500=4.38%, 750=0.01%
  lat (msec)   : 2=0.06%
  cpu          : usr=8.06%, sys=37.44%, ctx=25385, majf=0, minf=27
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=25600,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
   READ: bw=472MiB/s (495MB/s), 472MiB/s-472MiB/s (495MB/s-495MB/s), io=100MiB (105MB), run=212-212msec
```

