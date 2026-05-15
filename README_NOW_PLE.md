run Program
./bin/main

edit 
main.c

ลบแล้ว บิ้วใหม่ 
rm -rf build bin && cmake -S . -B build && cmake --build build 2>&1 

เปลี่ยนสี
static fx_block_t fx_rig[12] = {
    {"DRIVE", "ON", {0xFF,0x00, 0x00}, true},       // 0 แดง
    {"OVER\nDRIVE", "ON", {0xDF, 0x93, 0x1E}, true}, // 1 ส้ม -> ตัด 2 บรรทัดชัดเจน
    {"MOD", "OFF", {0x91, 0x2A, 0x8F}, false},       // 2 ม่วง
    {"DELAY", "ON", {0xD4, 0xEB, 0x6A}, true},      // 3 เขียวมะนาวสว่าง
    {"CHORUS", "ON", {0x00, 0x72, 0xC6}, true},     // 4 ฟ้า
    {"AMP", "CAB SIM", {0x50, 0x55, 0x55}, true},    // 5 เทา
    {"TREMOLO", "OFF", {0x2B, 0x82, 0x3E}, false},   // 6 เขียวเข้ม
    {"CAB", "4x12 CLS", {0x8C, 0x62, 0x39}, true},   // 7 น้ำตาล
    {"BOOST", "ON", {0x41, 0xEB, 0xEB}, true},      // 8 ฟ้า Cyan สว่าง
    {"REVERB", "ON", {0x8C, 0x31, 0x42}, true},     // 9 แดงก่ำ
    {"FILTER", "OFF", {0x23, 0xD4, 0x93}, false},   // 10 เขียวมิ้นต์สว่าง
    {"EQ", "ACTIVE", {0x6D, 0x82, 0x53}, true}      // 11 เขียวขี้ม้า
};