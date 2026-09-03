/**
 * 独家独家独家：单PC隐身Pico固件
 * 
 * 终极隐身方案：
 * - 一根USB线插5060，系统只看到一个普通鼠标
 * - 用HID Output Report偷偷收数据，Windows原生驱动，不需要额外驱动
 * - 没有CDC串口，没有vendor设备，没有可疑设备
 * - 系统设备管理器里只有一个HID-compliant mouse
 * - Vanguard看到的就是一个普通鼠标，连怀疑的理由都没有
 * 
 * 运输层：
 * - 鼠标移动：HID Input Report (标准)
 * - 坐标接收：HID Output Report (主机→设备，纯HID协议，系统原生支持)
 * - 伪装度：100%
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "tusb.h"

// ============================================================
// 配置
// ============================================================

#define REPORT_ID_MOUSE  1
#define REPORT_ID_COORD  2
#define COORD_REPORT_LEN 8

// 鼠标参数
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
    
    // 随机反应时间
    sleep_ms(rand_range(REACTION_MIN_MS, REACTION_MAX_MS));
    
    // 控制点随机偏移
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
    
    // 5% overshoot 更人类
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
// HID 协议
// ============================================================

// HID 报告描述符
// 两个报告ID：
// ID 1: 标准鼠标 (Input)
// ID 2: 坐标数据 (Output - 主机发给设备)
const uint8_t desc_hid_report[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x02,       // Usage (Mouse)
    0xA1, 0x01,       // Collection (Application)
    
    // Report ID 1: 鼠标输入
    0x85, REPORT_ID_MOUSE, // Report ID
    0x09, 0x01,       //   Usage (Pointer)
    0xA1, 0x00,       //   Collection (Physical)
    0x05, 0x09,       //     Usage Page (Button)
    0x19, 0x01,       //     Usage Minimum (1)
    0x29, 0x03,       //     Usage Maximum (3)
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x95, 0x03,       //     Report Count (3)
    0x75, 0x01,       //     Report Size (1)
    0x81, 0x02,       //     Input (Data,Var,Abs)
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x05,       //     Report Size (5)
    0x81, 0x01,       //     Input (Cnst,Arr,Abs)
    0x05, 0x01,       //     Usage Page (Generic Desktop)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x09, 0x38,       //     Usage (Wheel)
    0x15, 0x81,       //     Logical Minimum (-127)
    0x25, 0x7F,       //     Logical Maximum (127)
    0x95, 0x03,       //     Report Count (3)
    0x75, 0x08,       //     Report Size (8)
    0x81, 0x06,       //     Input (Data,Var,Rel)
    0xC0,             //   End Collection
    
    // Report ID 2: 坐标输出 (主机→设备, 隐形通道)
    0x85, REPORT_ID_COORD, // Report ID
    0x05, 0x01,       //     Usage Page (Generic Desktop)
    0x09, 0x00,       //     Usage (Undefined - 伪装成保留用法)
    0x15, 0x00,       //     Logical Minimum (0)
    0x26, 0xFF, 0x00, //     Logical Maximum (255)
    0x95, COORD_REPORT_LEN, // Report Count (8)
    0x75, 0x08,       // Report Size (8)
    0x91, 0x02,       // Output (Data,Var,Abs) - 主机到设备
    0xC0,             // End Collection
};

// HID Get Report 回调
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    return desc_hid_report;
}

// HID Set Report 回调 - 主机发数据给设备（核心！）
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize) {
    (void)instance;
    (void)report_type;
    
    if (report_id == REPORT_ID_COORD && bufsize >= COORD_REPORT_LEN) {
        // 解析坐标数据
        // 协议: [tx_high, tx_low, ty_high, ty_low, scx_high, scx_low, scy_high, scy_low]
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
    // 不初始化stdio，避免USB CDC被激活
    // stdio_init_all();
    
    rng_seed = to_ms_since_boot(get_absolute_time()) ^ 0xDEADBEEF;
    
    // 初始化TinyUSB
    tusb_init();
    
    // 等待USB枚举
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