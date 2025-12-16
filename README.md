## README: Sybera EtherCAT C++ Example Files

This document describes the C++ example files used in conjunction with the **Sybera EtherCAT Realtime Master Library (Cluster 64 Bit)**. These examples serve as a foundation for developing proprietary, deterministic EtherCAT Master applications under Windows.

---

### 1. Overview and Technology

The Sybera EtherCAT Master Library allows a standard PC running a Windows operating system and using a standard Ethernet adapter to function as a full-fledged EtherCAT Master. This is achieved through the **X-Realtime Engine**.

* **Realtime:** Enables deterministic control of EtherCAT Slave participants.
* **Cycle Times:** Realization of telegram update cycles down to **50 microseconds ($\mu$sec$)$**, depending on the hardware.
* **Hardware Connection:** Utilization of standard Ethernet chips (Intel or Realtek).
* **Synchronization:** Implementation of **Distributed Clock (DC) Management** for highly precise synchronization and compensation of jitter and signal propagation delay. The library supports "Dynamic Jitter Compensation."

### 2. Supported Development Environment and Prerequisites

The examples are designed for the following environments:

| Category | Details |
| :--- | :--- |
| **Operating Systems** | Windows 7, 8, 10, 11 |
| **Development Platforms** | Visual C++ (from version 2010), CVI LabWindows, Borland C++Builder |
| **Required** | Installed Sybera EtherCAT Master Library (www.sybera.com), compatible Ethernet hardware, EtherCAT Slave Information (ESI) file. |

### 3. Product Features (Master Functions)

The examples demonstrate the application of the library's comprehensive functions:

* **Protocol Management:**
    * FMMU (Fieldbus Memory Management Unit) Management
    * SYNC Management
    * PDO (Process Data Object) Assignment and Configuration
    * Distributed Clock (DC) Configuration
* **Mailbox Protocols:**
    * CoE (CANopen over EtherCAT) Management for Slave configuration (SDO access).
    * EoE (Ethernet over EtherCAT) Management.
* **Operation:** Intelligent station management, control of **State Management** (Init, Pre-Op, Safe-Op, Op), Watchdog Support.
* **Addressing:** Support for logical, physical, and alias addressing.

### 4. C++ API and Interfaces

The EtherCAT Realtime Master Library is integrated into C++ projects via a static link library. The example files demonstrate the usage of the two main interfaces:

| Interface | Description | Example Application |
| :--- | :--- | :--- |
| **Beginner Interface** (HighLevel) | For a quick and simple start. Provides convenient wrapper functions (e.g., `Sha64EcatCreate`, `Sha64EcatEnable`). | Simple initialization and cyclic I/O control. |
| **Expert Code** (LowLevel) | Enables full control over every single EtherCAT step, including detailed Command Functions, State Functions, and COE Functions. | In-depth configuration and complex state machines. |

#### 4.1 Key Library Files (Reference)

The following files are central to C++ programming with the library:

* **`SHA64ECATCORE.LIB` / `SHA64ECATCORE.DLL`**: The link and runtime libraries.
* **`SHA64ECATCORE.H`**: Contains all exported function prototypes.
* **`ECAT64COREDEF.H`**: Defines fundamental EtherCAT structures (e.g., `ECAT_PARAMS`, `STATION_INFO`).
* **`ECAT64SDODEF.H` / `ECAT64DCDEF.H`**: Specific definitions for CoE (SDO) and Distributed Clock.

---

### Further Information on the EtherCAT Master Library

Detailed descriptions of the functions, integrated tools (like the PDO Configurator), and technology (like Distributed Clock Management) can be found on the official Sybera product page:

Sybera EtherCAT Master for Windows
[Sybera EtherCAT Master für Windows](https://www.sybera.com/english/ethercat-master.htm)
