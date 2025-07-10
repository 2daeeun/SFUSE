### fio 워크플로우 (200MB 크기의 이미지 기반 파일시스템)

1. **순차 쓰기 → 순차 읽기**

   ```bash
   # 1) 순차 쓰기
   fio --name=seq-write \
       --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
       --size=100M --bs=1M --rw=write --direct=1 --ioengine=sync --iodepth=1

   # 2) 캐시 비우기
   echo 3 | sudo tee /proc/sys/vm/drop_caches

   # 3) 순차 읽기 (같은 파일)
   fio --name=seq-read \
       --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
       --size=100M --bs=1M --rw=read --direct=1 --ioengine=sync --iodepth=1
   ```

2. **랜덤 쓰기 → 랜덤 읽기**

   ```bash
   # 1) 랜덤 쓰기
   fio --name=rand-write \
       --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
       --size=100M --bs=4k --rw=randwrite --direct=1 --ioengine=libaio --iodepth=16

   # 2) 캐시 비우기
   echo 3 | sudo tee /proc/sys/vm/drop_caches

   # 3) 랜덤 읽기 (같은 파일)
   fio --name=rand-read \
       --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
       --size=100M --bs=4k --rw=randread --direct=1 --ioengine=libaio --iodepth=16
   ```
   
---
### SFUSE의 워크플로우 결과 (200MB 크기의 이미지 기반 파일시스템)
1. **SFUSE의 "순차 쓰기 → 순차 읽기"**
```bash
root@thinkpad:/run/media/leedaeeun/sfuse # 
fio --name=seq-write \
    --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
    --size=100M --bs=1M --rw=write --direct=1 --ioengine=sync --iodepth=1
seq-write: (g=0): rw=write, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process
seq-write: Laying out IO file (1 file / 100MiB)

seq-write: (groupid=0, jobs=1): err= 0: pid=229628: Thu Jul 10 16:05:55 2025
  write: IOPS=632, BW=633MiB/s (664MB/s)(100MiB/158msec); 0 zone resets
    clat (usec): min=1001, max=3570, avg=1530.46, stdev=491.97
     lat (usec): min=1042, max=3648, avg=1571.04, stdev=494.68
    clat percentiles (usec):
     |  1.00th=[ 1004],  5.00th=[ 1029], 10.00th=[ 1057], 20.00th=[ 1106],
     | 30.00th=[ 1188], 40.00th=[ 1237], 50.00th=[ 1303], 60.00th=[ 1500],
     | 70.00th=[ 1745], 80.00th=[ 2008], 90.00th=[ 2212], 95.00th=[ 2343],
     | 99.00th=[ 2671], 99.50th=[ 3556], 99.90th=[ 3556], 99.95th=[ 3556],
     | 99.99th=[ 3556]
  lat (msec)   : 2=79.00%, 4=21.00%
  cpu          : usr=2.55%, sys=3.18%, ctx=101, majf=0, minf=7
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,100,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
  WRITE: bw=633MiB/s (664MB/s), 633MiB/s-633MiB/s (664MB/s-664MB/s), io=100MiB (105MB), run=158-158msec
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```
```bash
root@thinkpad:/run/media/leedaeeun/sfuse # 
fio --name=seq-read \
    --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
    --size=100M --bs=1M --rw=read --direct=1 --ioengine=sync --iodepth=1
seq-read: (g=0): rw=read, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process

seq-read: (groupid=0, jobs=1): err= 0: pid=230085: Thu Jul 10 16:06:06 2025
  read: IOPS=793, BW=794MiB/s (832MB/s)(100MiB/126msec)
    clat (usec): min=737, max=3936, avg=1233.83, stdev=452.96
     lat (usec): min=737, max=3939, avg=1234.19, stdev=453.20
    clat percentiles (usec):
     |  1.00th=[  742],  5.00th=[  791], 10.00th=[  807], 20.00th=[  881],
     | 30.00th=[  922], 40.00th=[ 1020], 50.00th=[ 1188], 60.00th=[ 1254],
     | 70.00th=[ 1369], 80.00th=[ 1500], 90.00th=[ 1647], 95.00th=[ 1893],
     | 99.00th=[ 2638], 99.50th=[ 3949], 99.90th=[ 3949], 99.95th=[ 3949],
     | 99.99th=[ 3949]
  lat (usec)   : 750=1.00%, 1000=38.00%
  lat (msec)   : 2=57.00%, 4=4.00%
  cpu          : usr=0.00%, sys=6.40%, ctx=103, majf=1, minf=264
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=100,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
   READ: bw=794MiB/s (832MB/s), 794MiB/s-794MiB/s (832MB/s-832MB/s), io=100MiB (105MB), run=126-126msec
``` 

2. **SFUSE의 "랜덤 쓰기 → 랜덤 읽기"**
```bash
root@thinkpad:/run/media/leedaeeun/sfuse # 
fio --name=rand-write \
    --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randwrite --direct=1 --ioengine=libaio --iodepth=16
rand-write: (g=0): rw=randwrite, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process
rand-write: Laying out IO file (1 file / 100MiB)

rand-write: (groupid=0, jobs=1): err= 0: pid=232158: Thu Jul 10 16:06:59 2025
  write: IOPS=83.1k, BW=325MiB/s (340MB/s)(100MiB/308msec); 0 zone resets
    slat (nsec): min=723, max=800085, avg=11064.64, stdev=11391.25
    clat (usec): min=34, max=1917, avg=180.30, stdev=86.80
     lat (usec): min=44, max=1994, avg=191.37, stdev=91.71
    clat percentiles (usec):
     |  1.00th=[  139],  5.00th=[  143], 10.00th=[  145], 20.00th=[  149],
     | 30.00th=[  153], 40.00th=[  157], 50.00th=[  161], 60.00th=[  167],
     | 70.00th=[  172], 80.00th=[  178], 90.00th=[  184], 95.00th=[  363],
     | 99.00th=[  562], 99.50th=[  603], 99.90th=[ 1106], 99.95th=[ 1237],
     | 99.99th=[ 1795]
  lat (usec)   : 50=0.02%, 100=0.05%, 250=93.82%, 500=3.90%, 750=1.99%
  lat (usec)   : 1000=0.08%
  lat (msec)   : 2=0.14%
  cpu          : usr=13.03%, sys=22.48%, ctx=25408, majf=0, minf=8
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,25600,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
  WRITE: bw=325MiB/s (340MB/s), 325MiB/s-325MiB/s (340MB/s-340MB/s), io=100MiB (105MB), run=308-308msec
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```
```bash
root@thinkpad:/run/media/leedaeeun/sfuse # 
fio --name=rand-read \
    --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randread --direct=1 --ioengine=libaio --iodepth=16
rand-read: (g=0): rw=randread, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process

rand-read: (groupid=0, jobs=1): err= 0: pid=232557: Thu Jul 10 16:07:10 2025
  read: IOPS=106k, BW=413MiB/s (433MB/s)(100MiB/242msec)
    slat (nsec): min=621, max=1526.8k, avg=8606.84, stdev=12594.07
    clat (usec): min=49, max=1696, avg=141.78, stdev=75.80
     lat (usec): min=52, max=1706, avg=150.39, stdev=79.68
    clat percentiles (usec):
     |  1.00th=[  110],  5.00th=[  113], 10.00th=[  115], 20.00th=[  117],
     | 30.00th=[  119], 40.00th=[  122], 50.00th=[  129], 60.00th=[  135],
     | 70.00th=[  137], 80.00th=[  141], 90.00th=[  147], 95.00th=[  200],
     | 99.00th=[  529], 99.50th=[  603], 99.90th=[  734], 99.95th=[ 1598],
     | 99.99th=[ 1647]
  lat (usec)   : 50=0.01%, 100=0.38%, 250=95.45%, 500=3.02%, 750=1.04%
  lat (usec)   : 1000=0.03%
  lat (msec)   : 2=0.06%
  cpu          : usr=7.88%, sys=29.05%, ctx=25310, majf=0, minf=25
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=25600,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
   READ: bw=413MiB/s (433MB/s), 413MiB/s-413MiB/s (433MB/s-433MB/s), io=100MiB (105MB), run=242-242msec
```

