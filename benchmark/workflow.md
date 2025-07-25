### fio 워크플로우

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
### SFUSE의 워크플로우 결과
1. **SFUSE의 "순차 쓰기 → 순차 읽기"**
```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=seq-write \
    --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
    --size=100M --bs=1M --rw=write --direct=1 --ioengine=sync --iodepth=1
seq-write: (g=0): rw=write, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process
seq-write: Laying out IO file (1 file / 100MiB)
seq-write: (groupid=0, jobs=1): err= 0: pid=3896: Thu Jun 19 14:43:11 2025
  write: IOPS=321, BW=322MiB/s (337MB/s)(100MiB/311msec); 0 zone resets
    clat (usec): min=1891, max=6456, avg=3059.86, stdev=650.89
     lat (usec): min=1927, max=6494, avg=3085.39, stdev=650.82
    clat percentiles (usec):
     |  1.00th=[ 1893],  5.00th=[ 2278], 10.00th=[ 2376], 20.00th=[ 2507],
     | 30.00th=[ 2606], 40.00th=[ 2835], 50.00th=[ 2999], 60.00th=[ 3195],
     | 70.00th=[ 3359], 80.00th=[ 3490], 90.00th=[ 3687], 95.00th=[ 4228],
     | 99.00th=[ 4359], 99.50th=[ 6456], 99.90th=[ 6456], 99.95th=[ 6456],
     | 99.99th=[ 6456]
   bw (  KiB/s): min=204800, max=204800, per=62.20%, avg=204800.00, stdev= 0.00, samples=1
   iops        : min=  200, max=  200, avg=200.00, stdev= 0.00, samples=1
  lat (msec)   : 2=1.00%, 4=91.00%, 10=8.00%
  cpu          : usr=0.65%, sys=1.29%, ctx=102, majf=1, minf=9
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,100,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
  WRITE: bw=322MiB/s (337MB/s), 322MiB/s-322MiB/s (337MB/s-337MB/s), io=100MiB (105MB), run=311-311msec
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=seq-read \
    --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
    --size=100M --bs=1M --rw=read --direct=1 --ioengine=sync --iodepth=1
seq-read: (g=0): rw=read, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process
seq-read: (groupid=0, jobs=1): err= 0: pid=3918: Thu Jun 19 14:43:22 2025
  read: IOPS=440, BW=441MiB/s (462MB/s)(100MiB/227msec)
    clat (usec): min=1938, max=7356, avg=2247.42, stdev=828.74
     lat (usec): min=1938, max=7358, avg=2247.63, stdev=828.93
    clat percentiles (usec):
     |  1.00th=[ 1942],  5.00th=[ 1958], 10.00th=[ 1975], 20.00th=[ 1975],
     | 30.00th=[ 1991], 40.00th=[ 1991], 50.00th=[ 2008], 60.00th=[ 2024],
     | 70.00th=[ 2024], 80.00th=[ 2073], 90.00th=[ 2311], 95.00th=[ 3982],
     | 99.00th=[ 5669], 99.50th=[ 7373], 99.90th=[ 7373], 99.95th=[ 7373],
     | 99.99th=[ 7373]
  lat (msec)   : 2=47.00%, 4=48.00%, 10=5.00%
  cpu          : usr=0.00%, sys=1.77%, ctx=102, majf=1, minf=263
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=100,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
   READ: bw=441MiB/s (462MB/s), 441MiB/s-441MiB/s (462MB/s-462MB/s), io=100MiB (105MB), run=227-227msec
``` 

2. **SFUSE의 "랜덤 쓰기 → 랜덤 읽기"**
```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=rand-write \
    --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randwrite --direct=1 --ioengine=libaio --iodepth=16
rand-write: (g=0): rw=randwrite, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process
rand-write: Laying out IO file (1 file / 100MiB)
Jobs: 1 (f=1)
rand-write: (groupid=0, jobs=1): err= 0: pid=4129: Thu Jun 19 14:43:59 2025
  write: IOPS=30.9k, BW=121MiB/s (126MB/s)(100MiB/829msec); 0 zone resets
    slat (nsec): min=1392, max=915191, avg=30083.21, stdev=12825.93
    clat (usec): min=90, max=2657, avg=486.02, stdev=81.22
     lat (usec): min=116, max=2808, avg=516.10, stdev=85.55
    clat percentiles (usec):
     |  1.00th=[  355],  5.00th=[  392], 10.00th=[  412], 20.00th=[  437],
     | 30.00th=[  453], 40.00th=[  469], 50.00th=[  482], 60.00th=[  494],
     | 70.00th=[  510], 80.00th=[  529], 90.00th=[  562], 95.00th=[  586],
     | 99.00th=[  644], 99.50th=[  693], 99.90th=[ 1418], 99.95th=[ 2089],
     | 99.99th=[ 2376]
   bw (  KiB/s): min=77912, max=126888, per=82.90%, avg=102400.00, stdev=34631.26, samples=2
   iops        : min=19478, max=31722, avg=25600.00, stdev=8657.82, samples=2
  lat (usec)   : 100=0.01%, 250=0.17%, 500=62.55%, 750=36.96%, 1000=0.16%
  lat (msec)   : 2=0.10%, 4=0.06%
  cpu          : usr=8.70%, sys=14.73%, ctx=25511, majf=0, minf=11
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,25600,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
  WRITE: bw=121MiB/s (126MB/s), 121MiB/s-121MiB/s (126MB/s-126MB/s), io=100MiB (105MB), run=829-829msec
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=rand-read \
    --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randread --direct=1 --ioengine=libaio --iodepth=16
rand-read: (g=0): rw=randread, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process

rand-read: (groupid=0, jobs=1): err= 0: pid=4149: Thu Jun 19 14:44:08 2025
  read: IOPS=48.8k, BW=190MiB/s (200MB/s)(100MiB/525msec)
    slat (nsec): min=1267, max=590484, avg=18333.00, stdev=8627.64
    clat (usec): min=81, max=2057, avg=308.36, stdev=73.79
     lat (usec): min=99, max=2268, avg=326.69, stdev=77.85
    clat percentiles (usec):
     |  1.00th=[  206],  5.00th=[  235], 10.00th=[  243], 20.00th=[  265],
     | 30.00th=[  293], 40.00th=[  302], 50.00th=[  306], 60.00th=[  318],
     | 70.00th=[  326], 80.00th=[  334], 90.00th=[  363], 95.00th=[  396],
     | 99.00th=[  437], 99.50th=[  461], 99.90th=[ 1483], 99.95th=[ 1614],
     | 99.99th=[ 1942]
   bw (  KiB/s): min=195424, max=195424, per=100.00%, avg=195424.00, stdev= 0.00, samples=1
   iops        : min=48856, max=48856, avg=48856.00, stdev= 0.00, samples=1
  lat (usec)   : 100=0.02%, 250=15.32%, 500=84.29%, 750=0.13%, 1000=0.02%
  lat (msec)   : 2=0.22%, 4=0.01%
  cpu          : usr=11.83%, sys=22.52%, ctx=25543, majf=0, minf=25
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=25600,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
   READ: bw=190MiB/s (200MB/s), 190MiB/s-190MiB/s (200MB/s-200MB/s), io=100MiB (105MB), run=525-525msec
```

---
### Ext4의 워크플로우 결과
1. **Ext4의 "순차 쓰기 → 순차 읽기"**

```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=seq-write \
    --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
    --size=100M --bs=1M --rw=write --direct=1 --ioengine=sync --iodepth=1
seq-write: (g=0): rw=write, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process
seq-write: Laying out IO file (1 file / 100MiB)
seq-write: (groupid=0, jobs=1): err= 0: pid=4302: Thu Jun 19 14:45:59 2025
  write: IOPS=429, BW=429MiB/s (450MB/s)(100MiB/233msec); 0 zone resets
    clat (usec): min=2143, max=3641, avg=2245.26, stdev=142.03
     lat (usec): min=2195, max=3681, avg=2318.92, stdev=139.31
    clat percentiles (usec):
     |  1.00th=[ 2147],  5.00th=[ 2212], 10.00th=[ 2212], 20.00th=[ 2212],
     | 30.00th=[ 2212], 40.00th=[ 2245], 50.00th=[ 2245], 60.00th=[ 2245],
     | 70.00th=[ 2245], 80.00th=[ 2245], 90.00th=[ 2245], 95.00th=[ 2245],
     | 99.00th=[ 2278], 99.50th=[ 3654], 99.90th=[ 3654], 99.95th=[ 3654],
     | 99.99th=[ 3654]
  lat (msec)   : 4=100.00%
  cpu          : usr=4.74%, sys=8.19%, ctx=100, majf=0, minf=11
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,100,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
  WRITE: bw=429MiB/s (450MB/s), 429MiB/s-429MiB/s (450MB/s-450MB/s), io=100MiB (105MB), run=233-233msec

Disk stats (read/write):
  sdb: ios=0/61, sectors=0/124928, merge=0/0, ticks=0/130, in_queue=130, util=51.42%
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=seq-read \
    --filename=/run/media/leedaeeun/sfuse/fiotest.dat \
    --size=100M --bs=1M --rw=read --direct=1 --ioengine=sync --iodepth=1
seq-read: (g=0): rw=read, bs=(R) 1024KiB-1024KiB, (W) 1024KiB-1024KiB, (T) 1024KiB-1024KiB, ioengine=sync, iodepth=1
fio-3.39
Starting 1 process
seq-read: (groupid=0, jobs=1): err= 0: pid=4321: Thu Jun 19 14:46:09 2025
  read: IOPS=469, BW=469MiB/s (492MB/s)(100MiB/213msec)
    clat (usec): min=1911, max=2298, avg=2114.15, stdev=101.81
     lat (usec): min=1911, max=2298, avg=2114.67, stdev=102.04
    clat percentiles (usec):
     |  1.00th=[ 1909],  5.00th=[ 1942], 10.00th=[ 1958], 20.00th=[ 2008],
     | 30.00th=[ 2040], 40.00th=[ 2114], 50.00th=[ 2147], 60.00th=[ 2147],
     | 70.00th=[ 2147], 80.00th=[ 2212], 90.00th=[ 2245], 95.00th=[ 2245],
     | 99.00th=[ 2311], 99.50th=[ 2311], 99.90th=[ 2311], 99.95th=[ 2311],
     | 99.99th=[ 2311]
  lat (msec)   : 2=16.00%, 4=84.00%
  cpu          : usr=2.83%, sys=3.30%, ctx=101, majf=1, minf=266
  IO depths    : 1=100.0%, 2=0.0%, 4=0.0%, 8=0.0%, 16=0.0%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=100,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=1

Run status group 0 (all jobs):
   READ: bw=469MiB/s (492MB/s), 469MiB/s-469MiB/s (492MB/s-492MB/s), io=100MiB (105MB), run=213-213msec

Disk stats (read/write):
  sdb: ios=142/0, sectors=145408/0, merge=0/0, ticks=237/0, in_queue=237, util=56.22%
```

2. **Ext4의 "랜덤 쓰기 → 랜덤 읽기"**
```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=rand-write \
    --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randwrite --direct=1 --ioengine=libaio --iodepth=16
rand-write: (g=0): rw=randwrite, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process
rand-write: Laying out IO file (1 file / 100MiB)
rand-write: (groupid=0, jobs=1): err= 0: pid=4386: Thu Jun 19 14:46:57 2025
  write: IOPS=34.6k, BW=135MiB/s (142MB/s)(100MiB/740msec); 0 zone resets
    slat (usec): min=4, max=105, avg= 7.73, stdev= 2.63
    clat (usec): min=102, max=8738, avg=453.81, stdev=707.30
     lat (usec): min=108, max=8746, avg=461.53, stdev=707.33
    clat percentiles (usec):
     |  1.00th=[  184],  5.00th=[  212], 10.00th=[  217], 20.00th=[  221],
     | 30.00th=[  227], 40.00th=[  231], 50.00th=[  241], 60.00th=[  251],
     | 70.00th=[  265], 80.00th=[  347], 90.00th=[  725], 95.00th=[ 1876],
     | 99.00th=[ 3654], 99.50th=[ 4752], 99.90th=[ 7308], 99.95th=[ 8717],
     | 99.99th=[ 8717]
   bw (  KiB/s): min=136520, max=136520, per=98.66%, avg=136520.00, stdev= 0.00, samples=1
   iops        : min=34130, max=34130, avg=34130.00, stdev= 0.00, samples=1
  lat (usec)   : 250=59.57%, 500=26.28%, 750=4.40%, 1000=1.43%
  lat (msec)   : 2=3.86%, 4=3.66%, 10=0.79%
  cpu          : usr=7.98%, sys=29.91%, ctx=21227, majf=0, minf=11
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=0,25600,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
  WRITE: bw=135MiB/s (142MB/s), 135MiB/s-135MiB/s (142MB/s-142MB/s), io=100MiB (105MB), run=740-740msec

Disk stats (read/write):
  sdb: ios=4/22024, sectors=72/176192, merge=0/0, ticks=4/9836, in_queue=9839, util=71.05%
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
echo 3 | sudo tee /proc/sys/vm/drop_caches
3
```

```bash
root@arch:/run/media/leedaeeun/sfuse # 
fio --name=rand-read \
    --filename=/run/media/leedaeeun/sfuse/fiotest-rand.dat \
    --size=100M --bs=4k --rw=randread --direct=1 --ioengine=libaio --iodepth=16
rand-read: (g=0): rw=randread, bs=(R) 4096B-4096B, (W) 4096B-4096B, (T) 4096B-4096B, ioengine=libaio, iodepth=16
fio-3.39
Starting 1 process

rand-read: (groupid=0, jobs=1): err= 0: pid=4405: Thu Jun 19 14:47:03 2025
  read: IOPS=66.1k, BW=258MiB/s (271MB/s)(100MiB/387msec)
    slat (usec): min=3, max=102, avg= 4.71, stdev= 1.55
    clat (usec): min=109, max=1754, avg=236.32, stdev=58.07
     lat (usec): min=123, max=1778, avg=241.03, stdev=58.54
    clat percentiles (usec):
     |  1.00th=[  155],  5.00th=[  172], 10.00th=[  184], 20.00th=[  200],
     | 30.00th=[  212], 40.00th=[  223], 50.00th=[  231], 60.00th=[  241],
     | 70.00th=[  253], 80.00th=[  269], 90.00th=[  289], 95.00th=[  318],
     | 99.00th=[  363], 99.50th=[  392], 99.90th=[  510], 99.95th=[ 1729],
     | 99.99th=[ 1745]
  lat (usec)   : 250=67.60%, 500=32.26%, 750=0.08%
  lat (msec)   : 2=0.06%
  cpu          : usr=14.77%, sys=34.72%, ctx=21136, majf=0, minf=28
  IO depths    : 1=0.1%, 2=0.1%, 4=0.1%, 8=0.1%, 16=99.9%, 32=0.0%, >=64=0.0%
     submit    : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.0%, 32=0.0%, 64=0.0%, >=64=0.0%
     complete  : 0=0.0%, 4=100.0%, 8=0.0%, 16=0.1%, 32=0.0%, 64=0.0%, >=64=0.0%
     issued rwts: total=25600,0,0,0 short=0,0,0,0 dropped=0,0,0,0
     latency   : target=0, window=0, percentile=100.00%, depth=16

Run status group 0 (all jobs):
   READ: bw=258MiB/s (271MB/s), 258MiB/s-258MiB/s (271MB/s-271MB/s), io=100MiB (105MB), run=387-387msec

Disk stats (read/write):
  sdb: ios=9701/0, sectors=77608/0, merge=0/0, ticks=2248/0, in_queue=2248, util=40.16%
```