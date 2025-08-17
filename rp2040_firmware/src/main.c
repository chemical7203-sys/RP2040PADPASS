// Pico SDK Headers
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/uart.h"

// TinyUSB Headers
#include "tusb.h"

// lwIP Headers
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/dhcp.h"
#include "dhcpserver/dhcpserver.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/apps/httpd.h"

// Project-specific Headers
#include "protocol.h"

// --- Config Struct ---
typedef struct {
    bool invert_lx, invert_ly, invert_rx, invert_ry;
    uint8_t deadzone_l2, deadzone_r2;
    uint8_t pad_type; // 0: DS4 (generic), 1: Switch, 2: XInput (generic)
} GamepadConfig;

volatile GamepadConfig g_config = {
    .invert_lx = false, .invert_ly = false, .invert_rx = false, .invert_ry = false,
    .deadzone_l2 = 10, .deadzone_r2 = 10,
    .pad_type = 0
};

// --- UART & Data ---
#define UART_INSTANCE uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
volatile DS4InputData latest_ds4_data = {0};

// --- Network Config & State ---
static const uint8_t mac_address[6] = { 0x02, 0x02, 0x84, 0x6A, 0x96, 0x00 };
struct netif netif_data;

// --- Function Prototypes ---
void hid_task(void);
void net_task(void);
void uart_task(void);
void init_web_server(void);
const char * cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen);
void pack_switch_report(hid_switch_report_t report, const DS4InputData* data);

// --- Web Server Resources ---
static const tCGI cgi_handlers[] = { {"/config.cgi", cgi_handler} };
static const char *ssi_tags[] = { "inversion_lx", "inversion_ly", "inversion_rx", "inversion_ry", "deadzone_l2", "deadzone_r2", "type_ds4", "type_switch", "type_xinput" };

// --- TinyUSB & lwIP Callbacks (forward declarations) ---
err_t netif_init_cb(struct netif *netif);
err_t netif_output_cb(struct netif *netif, struct pbuf *p);

int main(void) {
    board_init();
    uart_init(UART_INSTANCE, BAUDRATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    tusb_init();
    init_web_server();
    while (1) {
        tud_task();
        net_task();
        uart_task();
        hid_task();
    }
    return 0;
}

void uart_task(void) {
    static SerialPacket packet_buffer;
    static uint8_t buffer_pos = 0;
    static enum { WAITING_FOR_START, RECEIVING_DATA, RECEIVING_CHECKSUM } state = WAITING_FOR_START;
    while (uart_is_readable(UART_INSTANCE)) {
        uint8_t byte = uart_getc(UART_INSTANCE);
        switch (state) {
            case WAITING_FOR_START: if (byte == START_BYTE) { buffer_pos = 1; state = RECEIVING_DATA; } break;
            case RECEIVING_DATA: ((uint8_t*)&packet_buffer)[buffer_pos++] = byte; if (buffer_pos >= sizeof(SerialPacket) - 1) { state = RECEIVING_CHECKSUM; } break;
            case RECEIVING_CHECKSUM:
                packet_buffer.checksum = byte;
                uint8_t calculated_checksum = 0;
                uint8_t* data_ptr = (uint8_t*)&packet_buffer.data;
                for (size_t i = 0; i < sizeof(DS4InputData); i++) {
                    calculated_checksum += data_ptr[i];
                }
                if (calculated_checksum == packet_buffer.checksum) {
                    uint32_t status = save_and_disable_interrupts();
                    latest_ds4_data = packet_buffer.data;
                    restore_interrupts(status);
                }
                state = WAITING_FOR_START;
                break;
        }
    }
}

void pack_switch_report(hid_switch_report_t report, const DS4InputData* data) {
    memset(report, 0, SWITCH_REPORT_SIZE);
    uint16_t buttons = 0;
    // Map DS4 buttons (based on common pygame indices) to Switch buttons
    if (data->buttons & (1 << 0)) buttons |= (1 << SWITCH_BTN_B);       // Cross -> B
    if (data->buttons & (1 << 1)) buttons |= (1 << SWITCH_BTN_A);       // Circle -> A
    if (data->buttons & (1 << 2)) buttons |= (1 << SWITCH_BTN_Y);       // Square -> Y
    if (data->buttons & (1 << 3)) buttons |= (1 << SWITCH_BTN_X);       // Triangle -> X
    if (data->buttons & (1 << 4)) buttons |= (1 << SWITCH_BTN_L);       // L1 -> L
    if (data->buttons & (1 << 5)) buttons |= (1 << SWITCH_BTN_R);       // R1 -> R
    if (data->buttons & (1 << 8)) buttons |= (1 << SWITCH_BTN_MINUS);   // Share -> Minus
    if (data->buttons & (1 << 9)) buttons |= (1 << SWITCH_BTN_PLUS);    // Options -> Plus
    if (data->buttons & (1 << 10)) buttons |= (1 << SWITCH_BTN_L_CLICK); // L3 -> L_CLICK
    if (data->buttons & (1 << 11)) buttons |= (1 << SWITCH_BTN_R_CLICK); // R3 -> R_CLICK
    if (data->buttons & (1 << 12)) buttons |= (1 << SWITCH_BTN_HOME);    // PS -> Home

    report[0] = buttons & 0xFF;
    report[1] = (buttons >> 8) & 0xFF;
    report[2] = HAT_SWITCH_NEUTRAL;

    uint16_t lx = (data->left_stick_x * 4095) / 255;
    uint16_t ly = (data->left_stick_y * 4095) / 255;
    uint16_t rx = (data->right_stick_x * 4095) / 255;
    uint16_t ry = (data->right_stick_y * 4095) / 255;

    // Correct packing based on reverse engineering doc
    report[3] = lx & 0xFF;
    report[4] = ((lx >> 8) & 0x0F) | ((ly & 0x0F) << 4);
    report[5] = (ly >> 4) & 0xFF;
    report[6] = rx & 0xFF;
    report[7] = ((rx >> 8) & 0x0F) | ((ry & 0x0F) << 4);
    report[8] = (ry >> 4) & 0xFF;
}

void hid_task(void) {
    const uint32_t interval_ms = 1;
    static uint32_t start_ms = 0;
    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    if (tud_suspended()) tud_remote_wakeup();
    if (!tud_hid_ready()) return;

    DS4InputData data;
    uint32_t status = save_and_disable_interrupts();
    data = latest_ds4_data;
    restore_interrupts(status);

    data.left_stick_x = g_config.invert_lx ? 255 - data.left_stick_x : data.left_stick_x;
    data.left_stick_y = g_config.invert_ly ? 255 - data.left_stick_y : data.left_stick_y;
    data.right_stick_x = g_config.invert_rx ? 255 - data.right_stick_x : data.right_stick_x;
    data.right_stick_y = g_config.invert_ry ? 255 - data.right_stick_y : data.right_stick_y;

    switch (g_config.pad_type) {
        case 1: {
            hid_switch_report_t report;
            pack_switch_report(report, &data);
            tud_hid_report(0, report, SWITCH_REPORT_SIZE);
            break;
        }
        default: {
            hid_gamepad_report_t report = {
                .x = (int8_t)(data.left_stick_x - 128), .y = (int8_t)(data.left_stick_y - 128),
                .z = (int8_t)(data.right_stick_x - 128), .rz = (int8_t)(data.right_stick_y - 128),
                .hat = 0, .buttons = (uint32_t)data.buttons
            };
            tud_hid_report(0, &report, sizeof(report));
            break;
        }
    }
}

void init_web_server(void) {
    ip4_addr_t ip, netmask, gw;
    IP4_ADDR(&ip, 192, 168, 7, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);
    lwip_init();
    netif_add(&netif_data, &ip, &netmask, &gw, NULL, netif_init_cb, netif_input);
    netif_set_default(&netif_data);
    netif_set_up(&netif_data);
    dhcp_server_init();
    httpd_init();
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
}

const char * cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]) {
    if (iIndex == 0) {
        g_config.invert_lx = g_config.invert_ly = g_config.invert_rx = g_config.invert_ry = false;
        for (int i = 0; i < iNumParams; i++) {
            if (strcmp(pcParam[i], "invert_lx") == 0) g_config.invert_lx = true;
            if (strcmp(pcParam[i], "invert_ly") == 0) g_config.invert_ly = true;
            if (strcmp(pcParam[i], "invert_rx") == 0) g_config.invert_rx = true;
            if (strcmp(pcParam[i], "invert_ry") == 0) g_config.invert_ry = true;
            if (strcmp(pcParam[i], "deadzone_l2") == 0) g_config.deadzone_l2 = atoi(pcValue[i]);
            if (strcmp(pcParam[i], "deadzone_r2") == 0) g_config.deadzone_r2 = atoi(pcValue[i]);
            if (strcmp(pcParam[i], "pad_type") == 0) {
                if (strcmp(pcValue[i], "ds4") == 0) g_config.pad_type = 0;
                else if (strcmp(pcValue[i], "switch") == 0) g_config.pad_type = 1;
                else if (strcmp(pcValue[i], "xinput") == 0) g_config.pad_type = 2;
            }
        }
    }
    return "/config.html";
}

u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
    switch (iIndex) {
        case 0: if (g_config.invert_lx) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 1: if (g_config.invert_ly) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 2: if (g_config.invert_rx) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 3: if (g_config.invert_ry) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 4: return snprintf(pcInsert, iInsertLen, "%d", g_config.deadzone_l2);
        case 5: return snprintf(pcInsert, iInsertLen, "%d", g_config.deadzone_r2);
        case 6: if (g_config.pad_type == 0) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 7: if (g_config.pad_type == 1) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 8: if (g_config.pad_type == 2) return snprintf(pcInsert, iInsertLen, "checked"); break;
    }
    return 0;
}

void net_task(void) { sys_check_timeouts(); }
err_t netif_init_cb(struct netif *netif) { netif->linkoutput = netif_output_cb; netif->output = etharp_output; netif->mtu = 1500; netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP | NETIF_FLAG_ETHERNET; return ERR_OK; }
err_t netif_output_cb(struct netif *netif, struct pbuf *p) {
    (void)netif;
    tud_network_xmit(p, 0);
    return ERR_OK;
}
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) { struct pbuf *p = (struct pbuf *)ref; return pbuf_copy_partial(p, dst, p->tot_len, 0); }
bool tud_network_recv_cb(const uint8_t *src, uint16_t size) { if (size) { struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL); if (p) { memcpy(p->payload, src, size); if (netif_data.input(p, &netif_data) != ERR_OK) { pbuf_free(p); } } } return true; }
void tud_network_mac_address_cb(uint8_t mac_addr[6]) { memcpy(mac_addr, mac_address, 6); }
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t r_id, hid_report_type_t rt, uint8_t* b, uint16_t rl) { return 0; }
void tud_hid_set_report_cb(uint8_t i, uint8_t r_id, hid_report_type_t rt, uint8_t const* b, uint16_t bl) {}
