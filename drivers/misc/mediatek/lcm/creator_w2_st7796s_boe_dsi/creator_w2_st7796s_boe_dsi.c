/*
 * Copyright (C) 2018 Yuvraj Saxena <xa@opcodex.me>
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lcm_drv.h"

/* Local Constants */

#define FRAME_WIDTH (320)
#define FRAME_HEIGHT (320)

#define REGFLAG_DELAY 0xFFE
#define REGFLAG_END_OF_TABLE 0xFFF

/* Local Variables */

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* Local Functions */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)                       \
        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)                          \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)                                     \
        lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)                                  \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

static struct LCM_setting_table {
        unsigned cmd;
        unsigned char count;
        unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
    {0x36, 0x01, {0x48}},
    {0x3A, 0x01, {0x77}},
    {0xF0, 0x01, {0xC3}},
    {0xF0, 0x01, {0x96}},
    {0xB1, 0x02, {0xA0, 0x10}},
    {0xB4, 0x01, {0x01}},
    {0xB5, 0x04, {0x40, 0x40, 0x00, 0x04}},
    {0xB6, 0x03, {0x8A, 0x07, 0x27}},
    {0xB9, 0x01, {0x02}},
    {0xC1, 0x01, {0x00}},
    {0xC5, 0x01, {0x28}},
    {0xE8, 0x08, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x93}},
    {0xE0,
     0x0E,
     {0xF0, 0x08, 0x0F, 0x0C, 0x09, 0x26, 0x3A, 0x44, 0x53, 0x39, 0x16, 0x17,
      0x34, 0x3F}},
    {0xE1,
     0x0E,
     {0xF0, 0x09, 0x0E, 0x08, 0x09, 0x25, 0x3B, 0x44, 0x53, 0x09, 0x16, 0x16,
      0x34, 0x3F}},
    {0x35, 0x01, {0x00}},
    {0x21, 0x01, {0x00}},
    {REGFLAG_DELAY, 0x78, {}},
    {0xF0, 0x01, {0x3C}},
    {0xF0, 0x01, {0x69}},
    {0x11, 0x01, {0x00}},
    {REGFLAG_DELAY, 0xC8, {}},
    {0x29, 0x01, {0x00}},
    {REGFLAG_DELAY, 0x32, {}},
    {0x2C, 0x00, {0x00}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    {0x28, 0x00, {0x00}},
    {REGFLAG_DELAY, 0x0A, {}},
    {0x10, 0x00, {0x00}},
    {REGFLAG_DELAY, 0x78, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}};

static void push_table(struct LCM_setting_table *table, unsigned int count,
                       unsigned char force_update) {
        unsigned int i;

        for (i = 0; i < count; i++) {

                unsigned cmd;
                cmd = table[i].cmd;

                switch (cmd) {

                case REGFLAG_DELAY:
                        MDELAY(table[i].count);
                        break;

                case REGFLAG_END_OF_TABLE:
                        break;

                default:
                        dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list,
                                        force_update);
                }
        }
}

/* LCM Driver Implementations */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util) {
        memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params) {
        memset(params, 0, sizeof(LCM_PARAMS));

        params->dsi.word_count = 960;
        params->dsi.vertical_active_line = 480;
        params->dsi.PLL_CLOCK = 221;
        params->dsi.lcm_esd_check_table[0].cmd = 10;
        params->dsi.lcm_esd_check_table[0].para_list[0] = -100;
        params->type = 2;
        params->dsi.data_format.format = 2;
        params->dsi.PS = 2;
        params->width = 320;
        params->height = 320;
        params->dbi.te_mode = 1;
        params->dsi.LANE_NUM = 1;
        params->dsi.ssc_disable = 1;
        params->dsi.esd_check_enable = 1;
        params->dsi.customization_esd_check_enable = 1;
        params->dsi.lcm_esd_check_table[0].count = 1;
        params->dbi.te_edge_polarity = 0;
        params->dsi.mode = 0;
        params->dsi.data_format.color_order = 0;
        params->dsi.data_format.trans_seq = 0;
        params->dsi.data_format.padding = 0;
        params->dsi.intermediat_buffer_num = 0;
        params->dsi.compatibility_for_nvk = 0;
        params->dsi.ssc_range = 0;
}

static void lcm_init(void) {
        SET_RESET_PIN(1);
        SET_RESET_PIN(0);
        MDELAY(10);
        SET_RESET_PIN(1);
        MDELAY(120);
        push_table(lcm_initialization_setting,
                   sizeof(lcm_initialization_setting) /
                       sizeof(struct LCM_setting_table),
                   1);
}

static void lcm_suspend(void) {
        push_table(lcm_deep_sleep_mode_in_setting,
                   sizeof(lcm_deep_sleep_mode_in_setting) /
                       sizeof(struct LCM_setting_table),
                   1);
}

static void lcm_resume(void) { lcm_init(); }

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
                       unsigned int height) {
        unsigned int x0 = x;
        unsigned int y0 = y;
        unsigned int x1 = x0 + width - 1;
        unsigned int y1 = y0 + height - 1;
        unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
        unsigned char x0_LSB = (x0 & 0xFF);
        unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
        unsigned char x1_LSB = (x1 & 0xFF);
        unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
        unsigned char y0_LSB = (y0 & 0xFF);
        unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
        unsigned char y1_LSB = (y1 & 0xFF);
        unsigned int data_array[7];

        data_array[0] = 0x00053902;
        data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
        data_array[2] = (x1_LSB);
        data_array[3] = 0x00053902;
        data_array[4] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
        data_array[5] = (y1_LSB);
        data_array[6] = 0x002c3909;
        dsi_set_cmdq(&data_array, 7, 0);
}

static unsigned int lcm_compare_id(void) {
        unsigned int array[1];

        SET_RESET_PIN(1);
        MDELAY(10);
        SET_RESET_PIN(0);
        MDELAY(20);
        SET_RESET_PIN(1);
        MDELAY(100);

        array[0] = 0x00043700;
        dsi_set_cmdq(array, 1, 1);
        MDELAY(10);
        read_reg_v2(0xD3, buffer, 4);

        return 1;
}

/* Get LCM Driver Hooks */

LCM_DRIVER creator_w2_st7796s_boe_dsi_lcm_drv = {
    .name           = "creator_w2_st7796s_boe_dsi",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
    .update         = lcm_update,
};
