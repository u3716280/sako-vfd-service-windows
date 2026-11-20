# วิธีติดตั้งฉบับย่อ — SAKO SKI780 ESP32 Service Tool v0.2.0

**บอร์ดที่ใช้:** Waveshare ESP32-S3-RS485-CAN รุ่นเสาอากาศในตัว (SKU 32154)  
**โปรแกรมพัฒนา:** VS Code + PlatformIO / Arduino C++  
**วัตถุประสงค์:** Monitoring / Parameter Library / JSON Backup และ Safe Restore ค่า RW จาก Windows v0.2 ผ่าน RS-485 โดยไม่มีคำสั่ง RUN/STOP มอเตอร์

## ติดตั้งลงบอร์ด

1. ติดตั้ง VS Code และ Extension **PlatformIO** บน PC
2. แตก ZIP และเปิดโฟลเดอร์ที่มี `platformio.ini`
3. คัดลอก `include/WiFiSecrets.h.example` เป็น `include/WiFiSecrets.h` แล้วใส่ `LAN_WIFI_SSID`, `LAN_WIFI_PASSWORD` และ `AP_WIFI_PASSWORD` สำหรับ AP สำรอง (ห้าม commit ไฟล์นี้ลง GitHub)
4. ใช้สาย USB-C แบบส่งข้อมูลต่อบอร์ดกับคอมพิวเตอร์
5. ใน PlatformIO สั่ง **Build** แล้ว **Upload** Firmware
6. สำคัญ: สั่ง **Upload Filesystem Image** เพิ่มอีกครั้ง เพื่อติดตั้งหน้าเว็บและ Parameter Library 448 รายการลง LittleFS
7. เปิด Serial Monitor 115200 เพื่อตรวจข้อมูลบอร์ด
8. ถ้า Router เชื่อมต่อสำเร็จ ให้ดู `LAN URL: http://192.168.x.x` ใน Serial Monitor แล้วเปิด IP นี้จากอุปกรณ์ใน LAN. ถ้าไม่มี Wi-Fi ตามที่กำหนด ให้ต่อ `SAKO-Service-v01` แล้วเข้า `http://192.168.4.1`
9. ทดสอบให้หน้าเว็บทำงานได้ก่อนต่อเข้ากับ VFD

## ต่อสาย RS-485

- ขั้ว **A+ ของ Waveshare → A+ ของ SKI780**
- ขั้ว **B− ของ Waveshare → B− ของ SKI780**
- ใช้ไฟเลี้ยง USB-C 5V จาก Power Bank หรือ DC 7–36V ตามขั้วที่บอร์ดกำหนด
- อย่าต่อกับขั้วไฟกำลังหรือสายมอเตอร์ของ VFD

ค่าเริ่มต้น Modbus: **9600, 8N2, Slave ID 1** และตัวอ่านเฟรมใช้ **Standard Modbus RTU** (ต้องตรวจว่าตั้งค่าใน SKI780 ให้รองรับตรงกัน)

## เมนูที่ใช้

**Dashboard:** Output Voltage, Output Current, Running Frequency, DC Bus Voltage, Estimated RPM, RUN/STOP, Forward/Reverse, Fault. ไม่แสดง Input AC Voltage เพราะไม่มี Register ตรงที่ยืนยันจากคู่มือ

**Parameters:** ค้นหา/กรองและดู Access Type (RO/RW/WO/Restricted/Unknown), Default, Min/Max, Unit, Description และกดอ่าน Raw Value ได้เฉพาะรายการ RO/RW ที่มี Address

**Parameter Library:** แสดง Definition 448 รายการ และจัดการ Profiles แบบ JSON ใน Browser (ต้อง Export เพื่อเก็บถาวร)

**JSON Backup:** สั่งอ่านค่าทั้งหมดที่กำหนด RO/RW แบบต่อเนื่อง ดาวน์โหลดเป็น JSON ที่มี `parameters`, `failed`, `complete` และ `counts` หากบาง Register อ่านไม่ได้จะระบุ `complete=false`

**Settings:** เปลี่ยน Slave, Baud, 8N2/8N1/8E1/8O1 ของ **ESP32 เท่านั้น** ไม่เปลี่ยน VFD

## ข้อควรตรวจสอบ

- บอร์ดจริงยังไม่ผ่านการทดสอบในสภาพแวดล้อมนี้ ไฟล์เป็น **Source Code ที่พร้อมนำไป Build/Test** ไม่ใช่ Firmware `.bin` ที่ยืนยันว่า Upload สำเร็จแล้ว
- Pinout ใน Source ใช้ `GPIO17 TX`, `GPIO18 RX`, `GPIO21 DE` และต้องยืนยันตาม Schematic/Revision จริง
- ใช้ Register U0 เพื่อคำนวณค่าบน Dashboard ตามค่าความละเอียดจากคู่มือ
- **Estimated RPM ไม่ใช่รอบจริงที่วัดด้วย Encoder/Tachometer**
- ไม่มีคำสั่ง RUN/STOP; Compare ทำบน Windows และ Restore รับเฉพาะ RW ที่ Firmware เชื่อถือ
- ฟังก์ชัน Restore ที่จะทำในอนาคตเขียนได้เฉพาะ Access `RW` เมื่อผ่านการตรวจสอบเงื่อนไข
- Firmware มีเพียง Wi-Fi WPA2 สำหรับการเข้าถึงหน้า Web ในรุ่นนี้ ไม่ควรเปิดสู่เครือข่ายสาธารณะ

รายละเอียด API, ปัญหาที่พบบ่อย และสถาปัตยกรรม ดูไฟล์ `README.md`

## LAN Wi-Fi Mode (v0.1.2)

- This board has **Wi-Fi**, not an Ethernet RJ45 LAN port. It connects to your router's **2.4 GHz Wi-Fi** and receives an IP via DHCP.
- Recommended: Reserve the assigned IP in your router using the ESP32 station MAC address. Avoid hardcoding a static IP unless you control subnet, gateway, and DHCP exclusion.
- Startup: try Station (STA) mode for 15 seconds; if unavailable, enable fallback AP `SAKO-Service-v01` at `192.168.4.1` and keep attempting STA reconnect.
- If STA disconnects during operation for 20 seconds, the firmware enables a fallback AP. If STA later reconnects, both interfaces may remain active until reboot.
- The Settings page and `/api/status` display `network_mode`, `network_ssid`, and `network_ip`. No Web/Modbus feature changes.
- **Security:** Firmware ไม่มี Device PIN, Web Login หรือ HTTPS ให้ใช้เฉพาะ LAN ส่วนตัวที่เชื่อถือได้ และห้ามทำ Port Forward ออก Internet

## Safe Restore v0.2.0

- รับเฉพาะ Parameter ที่ถูก Compile เป็น `RW` ใน Firmware; ไม่เชื่อชนิด Access จาก JSON
- ปฏิเสธ RO / WO / Restricted / Unknown และ Register กลุ่ม `REG_*`
- ก่อนทุกค่าต้องอ่านสถานะ VFD ได้ว่า STOP=3
- อ่านค่าปัจจุบันก่อน ถ้าเท่ากันจะไม่เขียนซ้ำ
- ค่าที่ต่างจะเขียนด้วย Modbus Function 0x06 และอ่านกลับด้วย 0x03 เพื่อตรวจสอบ
- หากค่าใดผิดพลาด Windows จะหยุด Restore ทันที
- ก่อนใช้งานจริงให้เก็บ Backup เดิมและทดลองกับ Parameter ที่ไม่สำคัญก่อน
- If you edit only `WiFiSecrets.h` and C++, upload **Firmware**; no need to reupload LittleFS. If you also want the updated Settings page, upload **Filesystem Image** too. Keep `partitions.csv` unchanged.

หมายเหตุสำหรับ GitHub: ไฟล์ `include/WiFiSecrets.h` ไม่ถูก commit เพื่อป้องกันข้อมูล Wi-Fi หลุด ให้คัดลอก `include/WiFiSecrets.h.example` เป็น `include/WiFiSecrets.h` หลัง Clone ก่อน Build
