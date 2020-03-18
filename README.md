# About

MESMERIC is a main memory emulator supporting a gap between read/write latencies.
It can emulate various read/write latencies of the main memory of a computer.
It enables us to investigate the impact of emerging memory devices on application performance.
It is a software program running on an ordinary DRAM-equipped machine. It is easy to setup.

- MESMERIC is light-weight. A target program runs in real time as if being
  executed on emulated main memory. In comparison to cycle accurate simulators
  (e.g., gem5), it is not necessary to wait for hours to have results.
  MESMERIC adjusts the task scheduling of a target application in real time.
  MESMERIC is usable for ordinary application programs, not limited to tiny
  benchmark programs.

- MESMERIC emulates both read and write latencies, respectively. In emerging
  memory devices such as PCM and ReRAM, the latency of a write operation is
  larger than that of a read operation. If such devices is incorporated
  into the main memory, there will be great impact on applications that are
  originally designed for DRAM-based main memory.
  In comparison to Quartz, MESMERIC allows users to specify read/write
  latencies respectively. It accurately emulates the performance of applications
  for such memory devices.

For details, please refer to the below paper.
- A Software-based NVM Emulator Supporting Read/Write Asymmetric Latencies,  
  Atsushi Koshiba, Takahiro Hirofuchi, Ryousei Takano, Mitaro Namiki,  
  IEICE Transactions on Information and Systems, pp.2377-2388, Vol.E102-D, No.12, IEICE, Dec 2019  
  DOI: 10.1587/transinf.2019PAP0018  
  [Download](http://doi.org/10.1587/transinf.2019PAP0018)


# Availability

The binary package of MESMERIC is available at [Download]() soon.
It is released under the below license.
```
Copyright (c) 2020 National Institute of Advanced Industrial Science and Technology (AIST)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

At this moment, the source code of MESMERIC is available for those who collaborate on research projects with us. 
It is also available under an agreement with AIST. Feel free to contact us.



# Usage

## Prerequisite

- Linux machine with Intel Xeon CPU
  - Intel Xeon will be necessary. Other lines of processors such as Intel Core
    may lack a necessary performance monitoring unit.
  - It is well-tested with processors of the Haswell and Broadwell
    architecture. It should work with processors of other architectures.
  - It uses the userland API (i.e., sysfs and system calls) of the Linux kernel.
    It is tested with Linux 5.4.0-rc2. It should work with other Linux versions
    as long as the API is compatible.

- The dynamic frequency control of processors should be disabled to obtain
  accurate results.
- The DRAM latency and the LLC hit latency of the host machine need to be
  measured in advance, for example by using Intel MLC.


## Command Line

```
sudo ./mes [ options ] <read latency (ns)> <write latency (ns)> [ <read latency (ns)> <write latency (ns)> [...] ]

Options:
-t <target path>
   The path of a target application program to be executed on the emulator.
-a <target arg>
   The argument given to the target application program.
   Multiple -a options are accepted.
-i <interval>
   The interval time in msec to read performance counters.
   The default value is 20 msec.
-c <cpu set>
   The mask of CPU cores reserved for the emulator.
   All the CPU cores are reserved.
-p <pebs sampling period>
   The sampling period of PEBS used for hybrid memory emulation.
-l <latency>
   The real access latency of DRAM observed on the host machine.
   The default value is 86.7 ns.
-w <weight>
   The weight of a cache miss penalty observed on the host machine,
   i.e., the ratio of the access latency of a LLC miss to that of a LLC hit.
   The default value is 4.2.
-f <frequency>
   The CPU frequency in MHz.
   If not specified, the value in /proc/cpuinfo is used.
-o
   The one-shot mode.
   The emulator exits when the target application given in the -t option exits.

Example:
sudo ./mes -t your_app_path 400 800
=> Execute your application, emulating a memory region of 400-ns read
   and 800-ns write latency.

sudo ./mes -t your_app_path -l 86.7 -w 4.2 400 800
=> Execute your application in the same manner as the above example.
   Explicitly specify the DRAM latency and the weight of a cache miss
   penalty of your host machine.

sudo ./mes -t your_app_path -l 86.7 -w 4.2 400 800 100 100
=> Emulate a hybrid memory system composed of 2 memory regions;
   a memory region has 400-ns read and 800-ns write latency
   and another memory region has 100-ns read/write latency.
```

The emulator provides an API for a target application in order to support
multi-threaded applications and hybrid memory emulation.
- If creating multiple threads/processes, a target application program needs to
  be slightly extended to inform the emulator of thread information.
- To emulate a hybrid memory system, a target application program needs to be
  slightly extended to inform the emulator of memory allocation.
- More documentation will come up soon.


# Contributors

- Takahiro Hirofuchi (AIST)
- Ryousei Takano (AIST)
- Atsushi Koshiba (Internship from TUAT, now RIKEN)
- Jeseong Yeon (Internship from Chungbuk National University)
- Youil Han (Internship from Chungbuk National University)


# Contact

- [Takahiro Hirofuchi (AIST)](https://takahiro-hirofuchi.github.io)


# Copyright

Copyright (c) 2020 National Institute of Advanced Industrial Science and Technology (AIST), Japan
