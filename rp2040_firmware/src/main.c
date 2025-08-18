/* Pico SDK Headers */
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/uart.h"

/* TinyUSB Headers */
#include "tusb.h"

/* lwIP Headers */
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/apps/httpd.h"
#include "dhserver.h"
#include "dnserver.h"

/* Project-specific Headers */
#include "protocol.h"

/* --- TYPE DEFINITIONS AND FORWARD DECLARATIONS --- */

// Web server configuration state
typedef struct {
    bool invert_lx, invert_ly, invert_rx, invert_ry;
    uint8_t deadzone_l2, deadzone_r2;
    uint8_t pad_type;
} GamepadConfig;

// Function Prototypes
void hid_task(void);
void uart_task(void);
void service_traffic(void);
bool dns_query_proc(const char *name, ip4_addr_t *addr);
u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen);
const char * cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
err_t netif_init_fn(struct netif *netif);

/* --- GLOBAL VARIABLES --- */

volatile GamepadConfig g_config = { .pad_type = 0, .deadzone_l2 = 10, .deadzone_r2 = 10 };
volatile DS4InputData latest_ds4_data = {0};
static struct netif netif_data;

// DHCP server entries
static dhcp_entry_t dhcp_entries[] = {
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 2), 24 * 60 * 60 },
};
static const dhcp_config_t dhcp_config = {
    .router = IPADDR4_INIT_BYTES(0, 0, 0, 0),
    .port = 67,
    .dns = IPADDR4_INIT_BYTES(192, 168, 7, 1),
    .domain = "usb",
    .num_entry = TU_ARRAY_SIZE(dhcp_entries),
    .entries = dhcp_entries
};

// Web server resources
const char *ssi_tags[] = { "inversion_lx", "inversion_ly", "inversion_rx", "inversion_ry", "deadzone_l2", "deadzone_r2", "type_ds4", "type_switch", "type_xinput" };
const tCGI cgi_handlers[] = { {"/config.cgi", cgi_handler} };

/* --- MAIN APPLICATION --- */

int main(void) {
    board_init();
    uart_init(uart0, BAUDRATE); // Use uart0 directly
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    tusb_init();

    while (!netif_is_up(&netif_data));
    while (dhserv_init(&dhcp_config) != ERR_OK);
    while (dnserv_init(IP_ADDR_ANY, 53, dns_query_proc) != ERR_OK);
    httpd_init();
    http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));

    while (1) {
        tud_task();
        service_traffic();
        uart_task();
        hid_task();
    }
    return 0;
}

/* --- TASKS --- */

void uart_task(void) {
    static SerialPacket packet_buffer;
    static uint8_t buffer_pos = 0;
    static enum { WAITING_FOR_START, RECEIVING_DATA, RECEIVING_CHECKSUM } state = WAITING_FOR_START;
    while (uart_is_readable(uart0)) {
        uint8_t byte = uart_getc(uart0);
        switch (state) {
            case WAITING_FOR_START: if (byte == START_BYTE) { buffer_pos = 1; state = RECEIVING_DATA; } break;
            case RECEIVING_DATA: ((uint8_t*)&packet_buffer)[buffer_pos++] = byte; if (buffer_pos >= sizeof(SerialPacket) - 1) { state = RECEIVING_CHECKSUM; } break;
            case RECEIVING_CHECKSUM:
                packet_buffer.checksum = byte;
                uint8_t calculated_checksum = 0;
                for (size_t i = 0; i < sizeof(DS4InputData); i++) { calculated_checksum += ((uint8_t*)&packet_buffer.data)[i]; }
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

void pack_switch_report(hid_switch_report_t report, const DS4InputData* data);

void hid_task(void) {
    const uint32_t interval_ms = 1;
    static uint32_t start_ms = 0;
    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    if (tud_suspended() || !tud_hid_ready()) return;

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

/* --- NETWORK INITIALIZATION AND CALLBACKS --- */

uint32_t sys_now(void) { return board_millis(); }
err_t linkoutput_fn(struct netif *netif, struct pbuf *p) { (void)netif; tud_network_xmit(p, 0); return ERR_OK; }
err_t ip4_output_fn(struct netif *netif, struct pbuf *p, const ip4_addr_t *addr) { return etharp_output(netif, p, addr); }
err_t netif_init_fn(struct netif *netif) {
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
  netif->state = NULL; netif->name[0] = 'E'; netif->name[1] = 'X';
  netif->linkoutput = linkoutput_fn; netif->output = ip4_output_fn;
  return ERR_OK;
}

void tud_network_init_cb(void) {
  ip4_addr_t ip, netmask, gw;
  IP4_ADDR(&ip, 192, 168, 7, 1);
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 0, 0, 0, 0);

  lwip_init();
  netif_add(&netif_data, &ip, &netmask, &gw, NULL, netif_init_fn, ip_input);
  netif_set_default(&netif_data);
  netif_set_up(&netif_data);
}

void service_traffic(void) { sys_check_timeouts(); }

bool tud_network_recv_cb(const uint8_t *dst, uint16_t len) {
  struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  if (p) {
    memcpy(p->payload, dst, len);
    if (ethernet_input(p, &netif_data) != ERR_OK) {
      pbuf_free(p);
    }
  }
  tud_network_recv_renew();
  return true; // Return bool as per function signature
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  struct pbuf *p = (struct pbuf *)ref;
  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

bool dns_query_proc(const char *name, ip4_addr_t *addr) {
  if (0 == strcmp(name, "gp2040.config")) {
    *addr = *netif_ip4_addr(&netif_data);
    return true;
  }
  return false;
}

/* --- WEB SERVER HANDLERS --- */

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
        case 4: return snprintf(pcInsert, iInsertLen, "%d", g_config.deadzone_l2); break;
        case 5: return snprintf(pcInsert, iInsertLen, "%d", g_config.deadzone_r2); break;
        case 6: if (g_config.pad_type == 0) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 7: if (g_config.pad_type == 1) return snprintf(pcInsert, iInsertLen, "checked"); break;
        case 8: if (g_config.pad_type == 2) return snprintf(pcInsert, iInsertLen, "checked"); break;
    }
    return 0;
}

void pack_switch_report(hid_switch_report_t report, const DS4InputData* data) {
    memset(report, 0, SWITCH_REPORT_SIZE);
    uint16_t buttons = 0;
    if (data->buttons & (1 << 0)) buttons |= (1 << SWITCH_BTN_B);
    if (data->buttons & (1 << 1)) buttons |= (1 << SWITCH_BTN_A);
    if (data->buttons & (1 << 2)) buttons |= (1 << SWITCH_BTN_Y);
    if (data->buttons & (1 << 3)) buttons |= (1 << SWITCH_BTN_X);
    if (data->buttons & (1 << 4)) buttons |= (1 << SWITCH_BTN_L);
    if (data->buttons & (1 << 5)) buttons |= (1 << SWITCH_BTN_R);
    if (data->buttons & (1 << 8)) buttons |= (1 << SWITCH_BTN_MINUS);
    if (data->buttons & (1 << 9)) buttons |= (1 << SWITCH_BTN_PLUS);
    if (data->buttons & (1 << 10)) buttons |= (1 << SWITCH_BTN_L_CLICK);
    if (data->buttons & (1 << 11)) buttons |= (1 << SWITCH_BTN_R_CLICK);
    if (data->buttons & (1 << 12)) buttons |= (1 << SWITCH_BTN_HOME);
    report[0] = buttons & 0xFF;
    report[1] = (buttons >> 8) & 0xFF;
    report[2] = HAT_SWITCH_NEUTRAL;
    uint16_t lx = (data->left_stick_x * 4095) / 255;
    uint16_t ly = (data->left_stick_y * 4095) / 255;
    uint16_t rx = (data->right_stick_x * 4095) / 255;
    uint16_t ry = (data->right_stick_y * 4095) / 255;
    report[3] = lx & 0xFF;
    report[4] = ((lx >> 8) & 0x0F) | ((ly & 0x0F) << 4);
    report[5] = (ly >> 4) & 0xFF;
    report[6] = rx & 0xFF;
    report[7] = ((rx >> 8) & 0x0F) | ((ry & 0x0F) << 4);
    report[8] = (ry >> 4) & 0xFF;
}

// Stubs for unused callbacks that are part of the HID API
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t r_id, hid_report_type_t rt, uint8_t* b, uint16_t rl) { return 0; }
void tud_hid_set_report_cb(uint8_t i, uint8_t r_id, hid_report_type_t rt, uint8_t const* b, uint16_t bl) {}
