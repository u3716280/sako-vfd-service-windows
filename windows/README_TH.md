# SAKO VFD Service — แอป Windows .NET Fluent v0.1

แอปสำหรับ Windows 11 / Windows 10 x64 ใช้ WPF .NET 10, WPF UI Fluent และ Edge WebView2
เพื่อค้นหา Waveshare ESP32-S3-RS485-CAN ที่อยู่ใน LAN แล้วเปิด Web Dashboard
จาก ESP32 โดยตรง ไม่ต้องจำ IP ของ ESP32

## ความสามารถ

- ปุ่ม **Scan LAN** ค้นหา ESP32 ใน IPv4 /24 ของ Network Adapter ที่กำลังใช้งาน
- ตรวจสอบ JSON ที่ `/api/status` เพื่อยืนยันว่าเป็น SAKO Service Tool
- เลือกอุปกรณ์จากรายการ หรือใส่ IP ด้วยตนเอง
- เปิด Dashboard, Parameters, Library และ Backup ของ ESP32 ผ่าน WebView2
- แยกสถานะ ESP32 Online และ VFD RS-485 Connected
- จำ IP ล่าสุดที่เชื่อมต่อไว้บน PC
- แอป v0.1 ไม่มีคำสั่ง Restore หรือ Write Modbus

## Build บน GitHub

1. สร้าง Repository ใหม่ใน GitHub ชื่อ `sako-vfd-service-windows`
2. อัปโหลด **เนื้อหาภายในโฟลเดอร์นี้ทั้งหมด** ไป branch `main`
3. เปิด Actions → Build SAKO VFD Windows Fluent App → Run workflow
4. ดาวน์โหลด Artifact ชื่อ `SakoVfdService-win-x64-portable`
5. แตก ZIP ทั้งหมด แล้วเปิด `SakoVfdService.exe` บน Windows

Workflow จะ Restore NuGet, Build, Publish self-contained และอัปโหลด ZIP ให้
ไม่ต้อง Build บน PC ของคุณ แต่เครื่องที่ใช้แอปต้องมี WebView2 Runtime
(ปกติ Windows 11 มีอยู่แล้ว)

## หมายเหตุ

- การ Build บน GitHub runner ไม่สามารถทดสอบการต่อ RS-485 กับ VFD จริงได้
- หากวง LAN ปิด Client Isolation หรืออยู่คนละ VLAN ค้นหาอัตโนมัติอาจไม่เจอ
- กรอก IP เองได้ หาก DHCP แจก IP ใหม่จะสแกนค้นหาใหม่
- HTTP API ของ ESP32 รุ่นปัจจุบันยังไม่มี Authentication: ใช้เฉพาะ LAN ที่เชื่อถือได้
- ขั้นตอนนี้ยังไม่มีการยืนยันผล Build จริงบน Windows runner
