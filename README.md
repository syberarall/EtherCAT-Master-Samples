## README: Sybera EtherCAT C++ Beispiel-Dateien

Dieses Dokument beschreibt die C++ Beispiel-Dateien, die in Verbindung mit der **Sybera EtherCAT Realtime Master Library (Cluster 64 Bit)** verwendet werden. Die Beispiele dienen als Grundlage für die Entwicklung eigener, deterministischer EtherCAT-Master-Anwendungen unter Windows.

---

### 1. Überblick und Technologie

Die Sybera EtherCAT Master Library ermöglicht es, einen Standard-PC mit Windows-Betriebssystem und einem Standard-Ethernet-Adapter als vollwertigen EtherCAT-Master zu nutzen. Dies wird durch die **X-Realtime Engine** erreicht.

* **Echtzeit:** Ermöglicht deterministische Steuerung von EtherCAT-Slaves.
* **Zykluszeiten:** Realisierung von Telegramm-Update-Zyklen von bis zu **50 Mikrosekunden ($\mu$sec$)$** je nach Hardware.
* **Hardware-Anbindung:** Nutzung von Standard-Ethernet-Chips (Intel oder Realtek).
* **Synchronisation:** Implementierung des **Distributed Clock (DC) Management** zur hochpräzisen Synchronisation und Kompensation von Jitter und Signallaufzeit. Die Bibliothek unterstützt die "Dynamic Jitter Compensation". 

### 2. Unterstützte Entwicklungsumgebung und Voraussetzungen

Die Beispiele sind für die folgenden Umgebungen konzipiert:

| Kategorie | Details |
| :--- | :--- |
| **Betriebssysteme** | Windows 7, 8, 10, 11, oder höher |
| **Entwicklungsplattformen** | Visual C++ (ab Version 2010), CVI LabWindows, Borland C++Builder |
| **Erforderlich** | Installierte Sybera EtherCAT Master Library (www.sybera.de), kompatible Ethernet-Hardware, EtherCAT Network Information (ENI/XML)-Datei. |

### 3. Produktmerkmale (Master-Funktionen)

Die Beispiele zeigen die Anwendung der umfassenden Funktionen der Bibliothek:

* **Protokoll-Management:**
    * FMMU (Fieldbus Memory Management Unit) Management
    * SYNC Management
    * PDO (Process Data Object) Assignment und Konfiguration
    * Distributed Clock (DC) Konfiguration
* **Mailbox-Protokolle:**
    * CoE (CANopen over EtherCAT) Management für die Slave-Konfiguration (SDO-Zugriff).
    * EoE (Ethernet over EtherCAT) Management.
* **Betrieb:** Intelligentes Stationsmanagement, Steuerung der **State Management** (Init, Pre-Op, Safe-Op, Op), Watchdog Support. 
* **Adressierung:** Unterstützung für logische, physikalische und Alias-Adressierung.

### 4. C++ API und Schnittstellen

Die EtherCAT Realtime Master Library wird über eine statische Link-Bibliothek in C++-Projekte eingebunden. Die Beispiel-Dateien demonstrieren die Nutzung der beiden Haupt-Schnittstellen:

| Schnittstelle | Beschreibung | Anwendung in Beispielen |
| :--- | :--- | :--- |
| **Beginner Interface** (HighLevel) | Für einen schnellen und einfachen Einstieg. Bietet komfortable Wrapper-Funktionen (z.B. `Sha64EcatCreate`, `Sha64EcatEnable`). | Einfache Initialisierung und zyklische I/O-Steuerung. |
| **Expert Code** (LowLevel) | Ermöglicht die vollständige Kontrolle über jeden einzelnen EtherCAT-Schritt, einschließlich detaillierter Command Functions, State Functions und COE Functions. | Tiefgreifende Konfiguration und komplexe Zustandsautomaten. |

#### 4.1 Wichtige Bibliotheksdateien (Referenz)

Die folgenden Dateien sind zentral für die C++-Programmierung:

* **`SHA64ECATCORE.LIB` / `SHA64ECATCORE.DLL`**: Die Link- und Laufzeitbibliotheken.
* **`SHA64ECATCORE.H`**: Enthält alle exportierten Funktionsprototypen.
* **`ECAT64COREDEF.H`**: Definiert die grundlegenden EtherCAT-Strukturen (z.B. `ECAT_PARAMS`, `STATION_INFO`).
* **`ECAT64SDODEF.H` / `ECAT64DCDEF.H`**: Spezielle Definitionen für CoE (SDO) und Distributed Clock.

---
Möchten Sie, dass ich ein spezifisches Beispiel für das LowLevel oder HighLevel Interface (z.B. für PDO-Mapping) beschreibe?
