# MUZUwm (Mouse UserZ Window Manager)

**A Lightweight, Mouse-First, Out-of-the-Box X11 Window Manager**  
***"User ใช้ง่าย สบายมือ แต่คน Dev บอกเลยว่า ‘むず’ (Muzu / โคตรยาก)"*** 

**MUZUwm คือ X11 Window Manager สไตล์ Floating/Tiling ผสมผสาน ที่ออกแบบมาเพื่อให้ผู้ใช้ควบคุมเดสก์ท็อปผ่านเมาส์ได้อย่างลื่นไหล รวดเร็ว และพร้อมใช้งานทันที (Out of the Box) โดยไม่ต้องพึ่งพาคีย์ลัดซับซ้อน**

ที่มาของชื่อ (Naming &amp; Lore) \* **MUZUwm:** ย่อมาจาก **M**ouse **U**ser\*\*Z\*\* **W**indow **M**anager \* **aka むず (Muzu):** คำสแลงภาษาญี่ปุ่นที่ตัดย่อมาจาก *Muzukashii (難しい)* แปลว่า **"ยาก"** \* **ความหมาย:** ประสบการณ์ของผู้ใช้ (UX) จะง่ายและเป็นธรรมชาติที่สุด แต่ความซับซ้อนเบื้องหลัง—ทั้งการดักจับ Double-tap Timer บน XCB, การคำนวณ Mouse Chording, การเรนเดอร์ Overlay ด้วย Cairo, และการรองรับมาตรฐาน EWMH/ICCCM—ตกเป็นภาระของนักพัฒนาที่ต้องสร้างมันขึ้นมาด้วยความยากระดับ "むず"

---

## 1. วิสัยทัศน์และปรัชญาของโครงการ (Vision &amp; Core Philosophy)

MUZwm ถูกออกแบบมาเพื่อปิดช่องว่างระหว่าง **Tiling Window Manager** (ที่ต้องจดจำคีย์ลัดคีย์บอร์ดจำนวนมาก) กับ **Desktop Environment ขนาดใหญ่** (เช่น GNOME, KDE ที่ใช้ทรัพยากรสูง) โดยมีเป้าหมายหลัก 3 ประการ:

1. **Mouse-First &amp; Zero Muscle Memory:** ผู้ใช้สามารถสั่งการทำงานหลักได้ด้วยมือเดียวผ่านเมาส์ (เปิดแอป, ย้าย, จัดขนาด, สลับ Workspace, ปิดหน้าต่าง)
2. **Work Out of the Box (OOTB):** ติดตั้งเสร็จใช้งานได้ทันที มี Titlebar สวยงาม, Taskbar, System Tray, Dock และหน้าตั้งค่า GUI โดยไม่ต้องเสียเวลาเขียนสคริปต์ Config เอง
3. **Ultra Lightweight on X11:** พัฒนาด้วยภาษา C ร่วมกับ **XCB** และ **Cairo** กินแรมเริ่มต้นเพียง 30–50 MB ตอบสนองฉับไวและลื่นไหล

---

## 2. All-in-One Launchpad (ศูนย์บัญชาการเต็มจอ)

แทนที่การใช้เมนูคลิกขวาแบบเดิมๆ MUZwm ใช้ **Launchpad เต็มหน้าจอ** ที่รวบรวมทุกฟังก์ชันไว้ในที่เดียว

### 2.1 กลไกการเปิด (Double-Tap Super)

- **Trigger:** แตะปุ่ม `Super` (Windows key) ติดกันสองครั้ง ภายในเวลา $200–250\text{ ms}$
- **ข้อดี:** 
  - ไม่ชนกับคีย์ลัดผสมมาตรฐาน (เช่น `Super + Enter`, `Super + d`)
  - ไม่ทำให้ปุ่ม Super เดี่ยวๆ เกิดอาการดีเลย์ขณะรอคีย์ถัดไป
  - ผู้ใช้กดเปิดได้สะดวกรวดเร็วโดยไม่ต้องมองแป้นพิมพ์

### 2.2 การจัดวางตามหลัก Fitts's Law

สมการ Fitts's Law:

$$
T = a + b \log_2\left(1 + \frac{D}{W}\right)
$$

MUZwm ใช้ประโยชน์จาก **"มุมและขอบจอทั้งสี่"** ซึ่งมีขนาดเป้าหมาย ($W$) กว้างเป็นอนันต์ ผู้ใช้สามารถสะบัดเมาส์ชนมุมจอได้ทันทีโดยไม่ต้องกังวลเรื่องเคอร์เซอร์หลุด:

```
[ Top-Left: Window Overview ] ---------------------- [ Top-Right: Power & Session ]
|                                                                                |
|    +----------------------------------------------------------------------+    |
|    |                      FAVORITES / APPS GRID                           |    |
|    |     [ Huge Tile ]    [ Huge Tile ]    [ Huge Tile ]    [ Huge Tile ] |    |
|    |                                                                      |    |
|    |     [ Huge Tile ]    [ Huge Tile ]    [ Huge Tile ]    [ Huge Tile ] |    |
|    +----------------------------------------------------------------------+    |
|                                                                                |
[ Bottom-Left: Settings/Theme ] ------------------- [ Bottom-Right: Quick Sliders ]
```

- **4 มุมทองคำ (The 4 Golden Corners):**
  - **Top-Left (Overview):** แสดงหน้าต่างทั้งหมดที่เปิดอยู่ (Exposé View)
  - **Top-Right (Power):** ปุ่มคำสั่ง Sleep, Restart, Shutdown, Lock
  - **Bottom-Left (Settings):** เปิดหน้าต่างตั้งค่า GUI ปรับแต่งธีมและความเร็วเมาส์
  - **Bottom-Right (Toggles &amp; Sliders):** สไลเดอร์ระดับเสียง, ความสว่าง, Wi-Fi, Bluetooth
- **แผงกลางจอ (Giant App Grid):**
  - ไอคอนขนาดใหญ่พิเศษ (96x96 หรือ 128x128 px)
  - จัดอันดับแอปที่ใช้งานบ่อยที่สุด (Most Used) ขึ้นก่อนโดยอัตโนมัติ
  - รองรับการคลิกขวาเพื่อปักหมุด (Pin to Favorites)
  - รองรับ Type-to-Filter ค้นหาแอปทันทีเมื่อเริ่มพิมพ์โดยไม่ต้องคลิกช่องค้นหา

---

## 3. การควบคุมด้วยเมาส์บน Desktop (Mouse Superpowers)

- **Drag-Anywhere Movement:** คลิกเมาส์ขวาค้างพร้อมคลิกซ้าย (R+L Chord) บนส่วนใดก็ได้ของหน้าต่างเพื่อลากย้าย ไม่จำเป็นต้องเล็ง Titlebar
- **Edge Bumping &amp; Snapping:**
  - ลากหน้าต่างชนขอบจอซ้าย/ขวา $\rightarrow$ ขยายแบบแบ่งครึ่ง (50/50 Tile)
  - ลากชนมุมจอทั้งสี่ $\rightarrow$ ขยายเป็น 1 ใน 4 หน้าจอ (Quarter Tile)
  - แสดงกรอบพรีวิวก่อนปล่อยเมาส์
- **Mouse Wheel Integration:**
  - หมุนลูกกลิ้งบน Titlebar $\rightarrow$ ปรับความโปร่งใส (Opacity) หรือพับหน้าต่าง (Window Shading)
  - หมุนลูกกลิ้งบนขอบจอซ้าย/ขวา $\rightarrow$ สลับ Workspace ก่อนหน้า/ถัดไป

---

## 4. สถาปัตยกรรมทางเทคนิค (Technical Architecture)

```
+--------------------------------------------------------------------+
|                               MUZwm                                |
|                                                                    |
|  +-----------------------------+  +-----------------------------+  |
|  |     xcb Event Loop          |  |      cairo / pango          |  |
|  |  (Double-Tap, Motion, Drag) |  |   (Decorations & Launchpad) |  |
|  +-----------------------------+  +-----------------------------+  |
|                                                                    |
|  +-----------------------------+  +-----------------------------+  |
|  |       EWMH / ICCCM          |  |     Built-in Taskbar        |  |
|  |   (Window Interoperability) |  |  (Systray, Clock, Pager)    |  |
|  +-----------------------------+  +-----------------------------+  |
+--------------------------------------------------------------------+
```

- **Core:** เขียนด้วยภาษา C เชื่อมต่อผ่าน **XCB (X C Binding)** เพื่อการประมวลผล Event แบบอะซิงโครนัสที่มีประสิทธิภาพสูง
- **Graphics:** ใช้ **Cairo Graphics** และ **Pango** วาด UI ขอบมุมโค้ง, แถบสไลเดอร์, และตัวหนังสือที่คมชัด
- **Standards:** รองรับ **EWMH / ICCCM** ทำให้หน้าต่างของเบราว์เซอร์, IDE และแอปเกม (Steam) วางตัวได้อย่างถูกต้องสมบูรณ์

---

## 5. แผนการพัฒนาขั้นต้น (MVP Roadmap)

1. **Phase 1: Basic Window Frame &amp; Event Loop**
   - จัดการ Reparenting หน้าต่าง และวาดกรอบ Titlebar พื้นฐาน
   - วางระบบ Drag-to-Move และ Drag-to-Resize
2. **Phase 2: Double-Tap Super &amp; Launchpad UI**
   - เขียน Timer ตรวจจับ Double-tap บนปุ่ม Super ใน XCB
   - เรนเดอร์ Overlay เต็มจอด้วย Cairo พร้อม Grid แสดงผลแอปจาก `.desktop` files
3. **Phase 3: Snapping &amp; Edge Detection**
   - ตรวจจับการชนขอบจอและแสดงกรอบพรีวิว Snapping (Half / Quarter)
4. **Phase 4: Built-in Components &amp; Configuration**
   - รวม Status Bar / Taskbar ขนาดเล็ก
   - สร้างหน้าต่างตั้งค่า GUI สำหรับปรับธีมและพฤติกรรมของเมาส์