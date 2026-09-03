/**
 * USB HID 鼠标通信固件
 * 表面：标准 USB 鼠标
 * 暗通道：HID Output Report 接收坐标数据
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "tusb.h"

// ============================================================
// 配置
// ============================================================

#define REPORT_ID_MOUSE  1
#define REPORT_ID_COORD  2
#define COORD_REPORT_LEN 8

#define MOUSE_MAX_STEP 40
#define MOUSE_JITTER 0.3f
#define REACTION_MIN_MS 50
#define REACTION_MAX_MS 150
#define MOVE_DELAY_MS 8

// ============================================================
// 全局变量
// ============================================================

static volatile int target_x = 0, target_y = 0;
static volatile int target_valid = 0;
static volatile int screen_cx = 960, screen_cy = 540;
static uint32_t rng_seed;

// ============================================================
// 随机数
// ============================================================

static uint32_t fast_rand(void) {
    rng_seed = rng_seed * 1103515245 + 12345;
    return (rng_seed / 65536) % 32768;
}

static int rand_range(int min, int max) {
    if (max <= min) return min;
    return min + fast_rand() % (max - min + 1);
}

static float rand_float(float min, float max) {
    return min + (float)fast_rand() / 32768.0f * (max - min);
}

// ============================================================
// 贝塞尔曲线鼠标移动
// ============================================================

typedef struct { float x, y; } Point2D;

static Point2D bezier(Point2D p0, Point2D p1, Point2D p2, float t) {
    float u = 1.0f - t;
    Point2D r;
    r.x = u*u*p0.x + 2*u*t*p1.x + t*t*p2.x;
    r.y = u*u*p0.y + 2*u*t*p1.y + t*t*p2.y;
    return r;
}

void human_mouse_move(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    
    float dist = sqrtf(dx*dx + dy*dy);
    int steps = (int)(dist / MOUSE_MAX_STEP) + 1;
    
    sleep_ms(rand_range(REACTION_MIN_MS, REACTION_MAX_MS));
    
    float cox = dx * rand_float(-0.3f, 0.3f);
    float coy = dy * rand_float(-0.2f, 0.2f);
    
    for (int i = 0; i < steps; i++) {
        float t = (float)(i+1) / steps;
        float prev_t = (float)i / steps;
        
        Point2D p0 = {0, 0};
        Point2D p2 = {(float)dx, (float)dy};
        Point2D p1 = {(float)dx/2 + cox, (float)dy/2 + coy};
        
        Point2D cur = bezier(p0, p1, p2, t);
        Point2D prev = bezier(p0, p1, p2, prev_t);
        
        int mx = (int)(cur.x - prev.x + rand_float(-MOUSE_JITTER, MOUSE_JITTER));
        int my = (int)(cur.y - prev.y + rand_float(-MOUSE_JITTER, MOUSE_JITTER));
        
        if (mx != 0 || my != 0) {
            tud_hid_mouse_report(REPORT_ID_MOUSE, 0, mx, my, 0, 0);
        }
        sleep_ms(rand_range(MOVE_DELAY_MS-2, MOVE_DELAY_MS+4));
    }
    
    if (fast_rand() % 20 == 0) {
        sleep_ms(rand_range(10, 30));
        tud_hid_mouse_report(REPORT_ID_MOUSE, 0, rand_range(-2, 2), rand_range(-2, 2), 0, 0);
    }
}

// ============================================================
// 瞄准逻辑
// ============================================================

void run_aim(void) {
    if (!target_valid) return;
    
    int dx = target_x - screen_cx;
    int dy = target_y - screen_cy;
    
    if (abs(dx) < 3 && abs(dy) < 3) return;
    
    human_mouse_move(dx, dy);
    target_valid = 0;
}

// ============================================================
// HID 报告描述符
// ============================================================

const uint8_t desc_hid_report[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x02,       // Usage (Mouse)
    0xA1, 0x01,       // Collection (Application)
    
    0x85, REPORT_ID_MOUSE,
    0x09, 0x01,
    0xA1, 0x00,
    0x05, 0x09,
    0x19, 0x01,
    0x29, 0x03,
    0x15, 0x00,
    0x25, 0x01,
    0x95, 0x03,
    0x75, 0x01,
    0x81, 0x02,
    0x95, 0x01,
    0x75, 0x05,
    0x81, 0x01,
    0x05, 0x01,
    0x09, 0x30,
    0x09, 0x31,
    0x09, 0x38,
    0x15, 0x81,
    0x25, 0x7F,
    0x95, 0x03,
    0x75, 0x08,
    0x81, 0x06,
    0xC0,
    
    0x85, REPORT_ID_COORD,
    0x05, 0x01,
    0x09, 0x00,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x95, COORD_REPORT_LEN,
    0x75, 0x08,
    0x91, 0x02,
    0xC0,
};

// ============================================================
// TinyUSB 回调
// ============================================================

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    return desc_hid_report;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize) {
    (void)instance;
    (void)report_type;
    
    if (report_id == REPORT_ID_COORD && bufsize >= COORD_REPORT_LEN) {
        int16_t tx = (int16_t)((buffer[0] << 8) | buffer[1]);
        int16_t ty = (int16_t)((buffer[2] << 8) | buffer[3]);
        int16_t scx = (int16_t)((buffer[4] << 8) | buffer[5]);
        int16_t scy = (int16_t)((buffer[6] << 8) | buffer[7]);
        
        target_x = tx;
        target_y = ty;
        if (scx > 0 && scy > 0) {
            screen_cx = scx;
            screen_cy = scy;
        }
        target_valid = 1;
    }
}

// ============================================================
// 主函数
// ============================================================

int main() {
    rng_seed = to_ms_since_boot(get_absolute_time());
    
    tusb_init();
    sleep_ms(1500);
    
    while (1) {
        tud_task();
        
        if (target_valid) {
            run_aim();
            sleep_ms(10);
        } else {
            sleep_ms(50);
        }
    }
    
    return 0;
}