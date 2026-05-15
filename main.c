#define SDL_MAIN_HANDLED
#include "lvgl/lvgl.h"
#include <unistd.h>
#include <string.h>

// โครงสร้างข้อมูลสำหรับจำลองปุ่มเอฟเฟกต์แต่ละก้อน
typedef struct {
    const char * name;
    const char * subtext;
    lv_color_t color;      // สีหลักตอนเปิด (ON)
    bool is_on;
} fx_block_t;

// กำหนดค่าเริ่มต้นของทั้ง 12 ปุ่ม 
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

// ฟังก์ชันสลับสถานะปุ่ม
static void update_button_style(lv_obj_t * btn, fx_block_t * fx, int index) {
    lv_obj_t * lbl_main = lv_obj_get_child(btn, 0);
    lv_obj_t * lbl_sub = lv_obj_get_child(btn, 1);

    if (fx->is_on) {
        lv_obj_set_style_bg_color(btn, fx->color, 0);
        
        // กลุ่มปุ่มสีสว่าง (3 = DELAY, 8 = BOOST, 10 = FILTER) -> บังคับข้อความสีดำเข้ม
        if (index == 3 || index == 8 || index == 10) {
            lv_obj_set_style_text_color(lbl_main, lv_color_make(0, 0, 0), 0);
            lv_obj_set_style_text_color(lbl_sub, lv_color_make(20, 20, 20), 0);
        } else {
            lv_obj_set_style_text_color(lbl_main, lv_color_make(255, 255, 255), 0);
            lv_obj_set_style_text_color(lbl_sub, lv_color_make(230, 230, 230), 0);
        }
        
        if (strcmp(fx->subtext, "OFF") == 0 || strcmp(fx->subtext, "ON") == 0) {
            lv_label_set_text(lbl_sub, "ON");
        }
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_make(35, 35, 35), 0);
        
        lv_obj_set_style_text_color(lbl_main, lv_color_make(130, 130, 130), 0);
        lv_obj_set_style_text_color(lbl_sub, lv_color_make(90, 90, 90), 0);
        
        if (strcmp(fx->subtext, "OFF") == 0 || strcmp(fx->subtext, "ON") == 0) {
            lv_label_set_text(lbl_sub, "OFF");
        }
    }
}

// ฟังก์ชันเมื่อคลิกปุ่มผ่านเมาส์จำลอง
static void fx_button_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    fx_block_t * fx = lv_event_get_user_data(e);
    
    int index = (intptr_t)lv_obj_get_user_data(btn);

    fx->is_on = !fx->is_on; 
    update_button_style(btn, fx, index);
}

// ฟังก์ชันหลักในการวาดหน้าจอ UI
void create_guitar_fx_ui(void) {
    // 1. พื้นหลังดำสนิท
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_make(10, 10, 10), 0);

    // 2. Header
    lv_obj_t * header_label = lv_label_create(lv_screen_active());
    lv_label_set_text(header_label, "AMP SIM & FX");
    lv_obj_set_style_text_color(header_label, lv_color_make(240, 240, 240), 0);
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_12, 0); 
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 8);

    // 3. Grid container
    lv_obj_t * grid_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(grid_container, 316, 198);
    lv_obj_align(grid_container, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_set_layout(grid_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(grid_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);

    lv_obj_set_style_bg_opa(grid_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_container, 0, 0);
    lv_obj_set_style_pad_all(grid_container, 0, 0);
    lv_obj_set_style_pad_row(grid_container, 2, 0);
    lv_obj_set_style_pad_column(grid_container, 2, 0);

    // 4. สร้างปุ่ม 12 ปุ่ม (4 คอลัมน์ x 3 แถว)
    for (int i = 0; i < 12; i++) {
        lv_obj_t * btn = lv_button_create(grid_container);
        lv_obj_set_size(btn, 76, 62);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_all(btn, 2, 0);

        lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(btn, 1, 0); // บีบช่องไฟแนวตั้งให้ขยับขึ้นมาพอดี

        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, fx_button_event_cb, LV_EVENT_CLICKED, &fx_rig[i]);

        // ชื่อเอฟเฟกต์ (แก้ไขมาใช้ font_14 เพื่อแก้ปัญหาตัวหนา และแก้บั๊กเปิดโหมดขยายคำให้ยอมขึ้นบรรทัดใหม่)
        lv_obj_t * lbl_main = lv_label_create(btn);
        lv_label_set_text(lbl_main, fx_rig[i].name);
        lv_obj_set_style_text_font(lbl_main, &lv_font_montserrat_14, 0); // สลับมาใช้ฟอนต์ 14 ที่หนาและคมชัดโดยไม่ต้องพึ่ง Subpx
        lv_label_set_long_mode(lbl_main, LV_LABEL_LONG_WRAP);            // เปลี่ยนเป็น LONG_WRAP เพื่อบังคับให้ตัดบรรทัดเมื่อเจอ \n
        lv_obj_set_width(lbl_main, 74);                                  // แผ่ความกว้างข้อความให้เต็มพื้นที่ปุ่ม
        lv_obj_set_style_text_align(lbl_main, LV_TEXT_ALIGN_CENTER, 0);

        // ตัวสถานะด้านล่าง
        lv_obj_t * lbl_sub = lv_label_create(btn);
        lv_label_set_text(lbl_sub, fx_rig[i].subtext);
        lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_10, 0);
        lv_label_set_long_mode(lbl_sub, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl_sub, 72);
        lv_obj_set_style_text_align(lbl_sub, LV_TEXT_ALIGN_CENTER, 0);

        update_button_style(btn, &fx_rig[i], i);
    }

    // 5. Footer
    lv_obj_t * footer_left = lv_label_create(lv_screen_active());
    lv_label_set_text(footer_left, "PRESET 12: DREAMSCAPE");
    lv_obj_set_style_text_color(footer_left, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_font(footer_left, &lv_font_montserrat_10, 0);
    lv_obj_align(footer_left, LV_ALIGN_BOTTOM_LEFT, 6, -4);

    lv_obj_t * footer_right = lv_label_create(lv_screen_active());
    lv_label_set_text(footer_right, "128 BPM | TUNER");
    lv_obj_set_style_text_color(footer_right, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_font(footer_right, &lv_font_montserrat_10, 0);
    lv_obj_align(footer_right, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
}

int main(void)
{
    lv_init();

    lv_display_t * disp = lv_sdl_window_create(320, 240);
    lv_indev_t * mouse = lv_sdl_mouse_create();
    lv_indev_t * mousewheel = lv_sdl_mousewheel_create();
    lv_indev_t * keyboard = lv_sdl_keyboard_create();

    (void)mouse;
    (void)mousewheel;
    (void)keyboard;
    (void)disp;

    create_guitar_fx_ui();

    while(1) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    return 0;
}