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

At this moment, MESMERIC is available for those who collaborate on research projects with us. 
It is also available under an agreement with AIST. Feel free to contact us.


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
