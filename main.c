#define SDL_MAIN_HANDLED
#include "lvgl/lvgl.h"
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#if defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>
#endif

// โครงสร้างข้อมูลสำหรับจำลองปุ่มเอฟเฟกต์แต่ละก้อน
typedef struct {
    const char * name;
    const char * subtext;
    uint8_t r;
    uint8_t g;
    uint8_t b;            // สีหลักตอนเปิด (ON) แยกช่องเพื่อคำนวณความสว่างง่าย
    bool is_on;
} fx_block_t;

// กำหนดค่าเริ่มต้นของทั้ง 12 ปุ่ม 
static fx_block_t fx_rig[12] = {
    {"N->", "ON",  0xFF, 0x00, 0x00, true},       // แดง
    {"VOL", "ON",  0x6D, 0x82, 0x53, true},       // เขียวขี้ม้า
    {"EQ",  "ON",  0x41, 0xEB, 0xEB, true},       // ฟ้า Cyan สว่าง
    {"WAH", "OFF", 0x91, 0x2A, 0x8F, false},      // ม่วง
    {"RVB", "ON",  0xDF, 0x93, 0x1E, true},       // ส้ม
    {"DLY", "ON",  0xD4, 0xEB, 0x6A, true},       // เขียวมะนาวสว่าง
    {"MOD", "OFF", 0x2B, 0x82, 0x3E, false},      // เขียวเข้ม
    {"CAB", "4x12 CLS", 0x8C, 0x62, 0x39, true}, // น้ำตาล
    {"AMP", "CAB SIM",  0x50, 0x55, 0x55, true},  // เทา
    {"DST", "ON",  0x8C, 0x31, 0x42, true},       // แดงก่ำ
    {"NR",  "OFF", 0x23, 0xD4, 0x93, false},      // เขียวมิ้นต์สว่าง
    {"PRE", "ON",  0x00, 0x72, 0xC6, true}        // ฟ้า
};

/* GUI button references so external input can toggle them */
static lv_obj_t * fx_btns[12] = {0};
static volatile bool fx_dirty[12] = {0};

#if defined(__APPLE__)
static void MyMIDIReadProc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon) {
    (void)readProcRefCon; (void)srcConnRefCon;
    const MIDIPacket *packet = &pktlist->packet[0];
    for (unsigned int i = 0; i < pktlist->numPackets; ++i) {
        const Byte *data = packet->data;
        UInt16 len = packet->length;
        unsigned int idx = 0;
        fprintf(stderr, "[MIDI] packet %u len=%u\n", i, (unsigned)len);
        while (idx + 2 < len) {
            unsigned char status = data[idx] & 0xF0;
            if (status == 0x90 || status == 0x80) { // Note On / Off
                unsigned char note = data[idx+1];
                unsigned char vel  = data[idx+2];
                fprintf(stderr, "[MIDI] Note %u status=0x%02x vel=%u\n", note, status, vel);
                int button_index = note - 60; // map MIDI notes 60..71 -> buttons 0..11
                if (button_index >= 0 && button_index < 12) {
                    if (status == 0x90 && vel > 0) fx_rig[button_index].is_on = true;
                    else fx_rig[button_index].is_on = false;
                    fx_dirty[button_index] = true;
                }
                idx += 3;
            } else if (status == 0xB0) { // Control Change
                unsigned char cc = data[idx+1];
                unsigned char val = data[idx+2];
                fprintf(stderr, "[MIDI] CC %u val=%u\n", cc, val);
                int button_index = cc % 12; // simple mapping: CC number -> button
                if (button_index >= 0 && button_index < 12) {
                    fx_rig[button_index].is_on = (val > 64);
                    fx_dirty[button_index] = true;
                }
                idx += 3;
            } else {
                // unknown/status we skip one byte to try to resync
                fprintf(stderr, "[MIDI] Unknown status 0x%02x at idx %u\n", data[idx], idx);
                idx += 1;
            }
        }
        packet = MIDIPacketNext(packet);
    }
}

static void midi_init(void) {
    MIDIClientRef client = 0;
    MIDIPortRef inPort = 0;
    OSStatus res = MIDIClientCreate(CFSTR("lv_midi_client"), NULL, NULL, &client);
    if (res != noErr) {
        fprintf(stderr, "[MIDI] MIDIClientCreate failed: %d\n", (int)res);
        return;
    }
    res = MIDIInputPortCreate(client, CFSTR("lv_in"), MyMIDIReadProc, NULL, &inPort);
    if (res != noErr) {
        fprintf(stderr, "[MIDI] MIDIInputPortCreate failed: %d\n", (int)res);
        return;
    }

    ItemCount srcCount = MIDIGetNumberOfSources();
    fprintf(stderr, "[MIDI] Found %u MIDI sources\n", (unsigned)srcCount);
    for (ItemCount i = 0; i < srcCount; ++i) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (src) {
            MIDIPortConnectSource(inPort, src, NULL);
            CFStringRef name = NULL;
            MIDIObjectGetStringProperty(src, kMIDIPropertyName, &name);
            if (name) {
                char buf[128];
                CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
                fprintf(stderr, "[MIDI] Connected source: %s\n", buf);
                CFRelease(name);
            } else {
                fprintf(stderr, "[MIDI] Connected source: (unknown)\n");
            }
        }
    }
}
#endif

// ฟังก์ชันสลับสถานะปุ่ม
static void update_button_style(lv_obj_t * btn, fx_block_t * fx, int index) {
    lv_obj_t * lbl_main = lv_obj_get_child(btn, 0);
    lv_obj_t * lbl_sub = lv_obj_get_child(btn, 1);

    if (!lbl_main) {
        fprintf(stderr, "[UI] update_button_style: missing lbl_main for index %d\n", index);
        return;
    }
    if (!lbl_sub) {
        fprintf(stderr, "[UI] update_button_style: missing lbl_sub for index %d\n", index);
        return;
    }

    if (fx->is_on) {
        /* สร้างพื้นปุ่มแบบสองสี (gradient แนวตั้ง) โดยใช้สีหลักและเฉดเข้มกว่า
         * จะได้ลักษณะ "สองโทน" บน-ล่าง ซึ่งอ่านง่ายและสวยงาม */
        lv_color_t bg = lv_color_make(fx->r, fx->g, fx->b);
        lv_color_t grad = lv_color_make((uint8_t)(fx->r * 60 / 100),
                                        (uint8_t)(fx->g * 60 / 100),
                                        (uint8_t)(fx->b * 60 / 100));

        lv_obj_set_style_bg_color(btn, bg, 0);
        lv_obj_set_style_bg_grad_color(btn, grad, 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_grad_stop(btn, 50, 0); /* ตำแหน่งกลางแบ่งสองโทน */

        /* คำนวณความสว่าง (luminance) เพื่อเลือกข้อความสีดำ/ขาวให้อ่านง่าย */
        int luminance = (299 * fx->r + 587 * fx->g + 114 * fx->b) / 1000;
        if (luminance > 160) {
            lv_color_t txt = lv_color_make(0, 0, 0);
            lv_obj_set_style_text_color(lbl_main, txt, 0);
            lv_obj_set_style_text_outline_stroke_width(lbl_main, 1, 0);
            lv_obj_set_style_text_outline_stroke_color(lbl_main, txt, 0);
            lv_obj_set_style_text_outline_stroke_opa(lbl_main, LV_OPA_COVER, 0);
        } else {
            lv_color_t txt = lv_color_make(255, 255, 255);
            lv_obj_set_style_text_color(lbl_main, txt, 0);
            lv_obj_set_style_text_outline_stroke_width(lbl_main, 1, 0);
            lv_obj_set_style_text_outline_stroke_color(lbl_main, txt, 0);
            lv_obj_set_style_text_outline_stroke_opa(lbl_main, LV_OPA_COVER, 0);
        }
    } else {
        /* ปิดปุ่ม: เคลียร์ gradient เก่า แล้วตั้งพื้นเป็นดำเต็ม ตรงนี้แก้บั้กกรณีพื้นไม่เปลี่ยน */
        lv_obj_set_style_bg_color(btn, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_grad_color(btn, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_bg_grad_stop(btn, 0, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

        lv_color_t off_txt = lv_color_make(255, 255, 255);
        lv_obj_set_style_text_color(lbl_main, off_txt, 0);
        lv_obj_set_style_text_outline_stroke_width(lbl_main, 1, 0);
        lv_obj_set_style_text_outline_stroke_color(lbl_main, off_txt, 0);
        lv_obj_set_style_text_outline_stroke_opa(lbl_main, LV_OPA_COVER, 0);
        lv_obj_set_style_text_letter_space(lbl_main, 2, 0);
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
        fx_btns[i] = btn;

        // ชื่อเอฟเฟกต์ (แก้ไขมาใช้ font_14 เพื่อแก้ปัญหาตัวหนา และแก้บั๊กเปิดโหมดขยายคำให้ยอมขึ้นบรรทัดใหม่)
        lv_obj_t * lbl_main = lv_label_create(btn);
        lv_label_set_text(lbl_main, fx_rig[i].name);
        lv_obj_set_style_text_font(lbl_main, &lv_font_montserrat_14, 0); // สลับมาใช้ฟอนต์ 14 ที่หนาและคมชัดโดยไม่ต้องพึ่ง Subpx
        lv_label_set_long_mode(lbl_main, LV_LABEL_LONG_WRAP);            // เปลี่ยนเป็น LONG_WRAP เพื่อบังคับให้ตัดบรรทัดเมื่อเจอ \n
        lv_obj_set_width(lbl_main, 74);                                  // แผ่ความกว้างข้อความให้เต็มพื้นที่ปุ่ม
        lv_obj_set_style_text_align(lbl_main, LV_TEXT_ALIGN_CENTER, 0);

        // ตัวสถานะด้านล่าง (ซ่อน เพื่อไม่แสดงคำว่า ON/OFF)
        lv_obj_t * lbl_sub = lv_label_create(btn);
        lv_label_set_text(lbl_sub, "");
        lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_10, 0);
        lv_label_set_long_mode(lbl_sub, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl_sub, 72);
        lv_obj_set_style_text_align(lbl_sub, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_add_flag(lbl_sub, LV_OBJ_FLAG_HIDDEN);

        /* ทำให้ชื่อปุ่มหนาขึ้นด้วยการเพิ่ม letter spacing
         * (โปรดสังเกต: โปรเจกต์นี้มี montserrat_14 เป็นฟอนต์ใหญ่สุดที่คอนฟิกไว้) */
        lv_obj_set_style_text_letter_space(lbl_main, 1, 0);
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
    
#if defined(__APPLE__)
    midi_init();
#endif

    while(1) {
        lv_timer_handler();
        /* Apply any pending MIDI-driven changes on LVGL thread */
        for (int i = 0; i < 12; ++i) {
            if (fx_dirty[i] && fx_btns[i]) {
                update_button_style(fx_btns[i], &fx_rig[i], i);
                fx_dirty[i] = false;
            }
        }
        lv_delay_ms(5);
    }

    return 0;
}