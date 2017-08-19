#ifndef _IT668x_DRV_H_
#define _IT668x_DRV_H_

#define IT668X_HDMI_RX_ADDR 0x90
#define IT668X_HDMI_TX_ADDR 0x98
#define IT668X_MHL_ADDR 0xC8
#define IT668X_IIC_NUMBER   2

#define delay1ms(ms) msleep(ms)
#define hold1ms(ms)  mdelay(ms)

/* #define MHL_2X_3D */
#define MHL_HDCP_EN
/* #define EN_DDC_BYPASS */

/* ///////////////////////////////////////////////////////////// */
/* IT668x debug define */
/*  */
/* ///////////////////////////////////////////////////////////// */
/* #define DEBUT_HDMI */
/* #define DEBUG_MHL */
/* #define DEBUG_INT */

/* /////////////////////////////////////////////// */
/* IT668x function */
/*  */
/*  */
/*  */
/* /////////////////////////////////////////////// */
#define DDC_READ_USE_BUSY_WAIT
/* #define CBUS_CTS_OPT */
/* #define CTS_FORCE_OUTPUT */

#define F_MODE_RGB24  0
#define F_MODE_RGB444 0
#define F_MODE_YUV422 1
#define F_MODE_YUV444 2
#define F_MODE_CLRMOD_MASK 3

#define COLOR_DEPTH_8BIT         0
#define COLOR_DEPTH_10BIT        1
#define COLOR_DEPTH_12BIT        2
#define COLOR_DEPTH_16BIT        3

#define COLOR_RGB444          0
#define COLOR_YCbCr422        1
#define COLOR_YCbCr444        2

#define COLOR_RANGE_VESA         0
#define COLOR_RANGE_CEA          1

#define YCBCR_COEF_ITU601          0x01
#define YCBCR_COEF_ITU709          0x10

#define PICAR_NO        0
#define PICAR4_3        1
#define PICAR16_9       2

#define ACTAR_PIC       8
#define ACTAR4_3        9
#define ACTAR16_9       10
#define ACTAR14_9       11

#define FRMEPKT         0
#define SBSFULL         3
#define TOPBTM          6
#define SBSHALF         8



#define DISABLE_PACKET_PIXEL_MODE 0
#define AUTO_PACKET_PIXEL_MODE  1
#define FORCE_PACKET_PIXEL_MODE 2



#define HDPCD_FIFO_READ 0
#define HDPCD_REGISTER_READ 1
/* ///////////////////////////////////////////////////////////// */
/* hdmirx register list */
/*  */
/* ///////////////////////////////////////////////////////////// */
#define REG_HDMIRX_VERSION 0x04

#define REG_HDMIRX_IRQ0 0x05
#define INT_CLK_STABLE_CHG (1<<7)
#define INT_CLK_CHG_DETECT (1<<4)
#define INT_HDMI_MODE_CHG (1<<1)
#define INT_PWR5V_CHG     (1<<0)

#define REG_HDMIRX_INPUT_STS 0x06
#define B_RX_CLK_STABLE (1<<6)
#define B_RX_CLK_SPEED  (1<<4)
#define B_RX_CLK_VAILD  (1<<3)
#define B_RX_HDMI_MODE  (1<<2)
#define B_RX_PWR5V_ON	(1<<0)
#define B_RXLCLK_READY (B_RX_CLK_STABLE|B_RX_CLK_VAILD)


#define REG_HDMIRX_CLK_CTRL 0x07
#define B_EN_GVCLK (1<<3)

#define REG_HDMIRX_RESET   0x08
#define EN_CD_RESET   0x24

#define REG_HDMIRX_TMDS_POW 0x0A
#define SET_RXTMDS_OFF 0x82


#define REG_HDMIRX_IO_CONFIG 0x0E

#define B_INT_IO_TYPE (1<<6)
#define IO_INT_OPEN_DRAIN (1<<6)
#define IO_INT_PUSH_PULL  (0<<6)

#define B_INT_ACT_POPARITY (1<<5)
#define IO_INT_ACT_LOW	(1<<5)
#define IO_INT_ACT_HIGH  (0<<5)


#define REG_HDMIRX_IRQ0_MASK 0x0F

#define REG_HDMIRX_HPD_CTRL 0x14
#define M_SET_HPD 0x30
#define SET_HPD_LOW 0x20
#define SET_HPD_HIGH 0x30


#define REG_HDMIRX_TX_ADDRESS 0x2B

/* ///////////////////////////////////////////////////////////// */
/* hdmitx reg list */
/*  */
/* ///////////////////////////////////////////////////////////// */


#define MASK_ALL_BIT 0xFF

#define REG_INT_STATUS 0XF0
#define INT_U3_WAKEUP   (1<<4)
#define INT_CBUS_SETINT (1<<3)
#define INT_CBUS        (1<<2)
#define INT_HDMI        (1<<0)

#define REG_VENDER_ID_L 0x00
#define REG_VENDER_ID_H 0x01
#define REG_DEVICE_ID_L 0x02
#define REG_VEVICE_ID_H 0x03

#define REG_MHL_VERSION 0x03


#define REG_HDMITX_SWREST 0x04
#define B_ASYNC_RESET		(1<<6)
#define B_REST_RCLK         (1<<5)
#define B_REST_VIDEO        (1<<3)
#define B_REST_HDCP         (1<<0)
#define M_RESET_ALL 0x1D

#define REG_HDMITX_INT_IO  0x05

#define B_ENA_MHLTX_INT		(1<<5)
#define B_FORCE_HPD        (1<<4)


#define REG_HDMITX_INT_06 0x06
#define INT_VSYNC_CHG		(1>>7)
#define INT_VIDEO_STABLE    (1<<6)
#define INT_VIDEO_UNSTABLE  (1<<3)
#define INT_3D_PKT_LOSS     (1<<1)
#define INT_3D_PKT_PRESENT  (1<<0)


#define REG_HDMITX_INT_07 0x07
#define INT_V2H_FIFO_ERR    (1<<6)
#define INT_DDC_NOACK		(1<<5)
#define INT_DDC_FIFO_ERROR	(1<<4)
#define INT_RXSEN			(1<<2)
#define INT_HPD             (1<<1)
#define INT_VIDEO_PARA_CHG	(1<<0)



#define REG_HDMITX_INT_08 0x08
#define INT_HDCP_SYNC_DETFAIL	(1<<5)
#define INT_HDCP_PJ_CHK			(1<<4)
#define INT_HDCP_RI_CHK			(1<<3)
#define INT_HDCP_KSVLIST_CHK	(1<<2)
#define INT_HDCP_AUTH_DONE	(1<<1)
#define INT_HDCP_AUTH_FAIL	(1<<0)



#define REG_HDMITX_INT_06_MASK 0x09

#define B_MASK_VSYNC_CHG		(1>>7)
#define B_MASK_VIDEO_STABLE    (1<<6)
#define B_MASK_VIDEO_UNSTABLE  (1<<3)
#define B_MASK_INT_3D_PKT_LOSS (1<<1)
#define B_MASK_INT_3D_PKT_PRESENT (1<<0)



#define REG_HDMITX_INT_07_MASK 0x0A
#define B_MASK_V2H_FIFO_ERR    (1<<6)
#define B_MASK_DDC_NOACK		(1<<5)
#define B_MASK_DDC_FIFO_ERROR	(1<<4)
#define B_MASK_RXSEN			(1<<2)
#define B_MASK_HPD             (1<<1)
#define B_MASK_VIDEO_PARA_CHG	(1<<0)

#define REG_HDMITX_INT_08_MASK 0x0B
#define B_MASK_HDCP_SYNC_DETFAIL	(1<<5)
#define B_MASK_HDCP_PJ_CHK			(1<<4)
#define B_MASK_HDCP_RI_CHK			(1<<3)
#define B_MASK_HDCP_KSVLIST_CHK		(1<<2)
#define B_MASK_HDCP_AUTH_DONE		(1<<1)
#define B_MASK_HDCP_AUTH_FAIL		(1<<0)


#define REG_HDMITX_VIDEO_STATUS 0x0E
#define B_HPD_DETECT    (1<<6)
#define B_RXSEN_DETECT  (1<<5)
#define B_VIDEO_STABLE  (1<<4)

#define REG_HDMITX_BANK_SEL 0x0F
#define REG_HDMITX_GATE_CLK 0x0F
#define B_GATE_RCLK  (1<<6)
#define B_GATE_IACLK (1<<5)
#define B_GATE_TXCLK (1<<4)
#define M_BANK_SEL 0x03

#define REG_HDMITX_DDC_MASTER_SEL 0x10
#define M_DDC_MASTER    1
#define B_DDC_PC_MASTER 1
#define B_DDC_HDCP_MASTER 0

#define REG_HDMITX_DDC_HEADER 0x11
#define DDC_HEADER_HDCP 0x74
#define DDC_HEADER_EDID 0xA0

#define REG_HDMITX_DDC_OFFSET 0x12
#define REG_HDMITX_DDC_BYTE_NUMBER 0x13
#define REG_HDMITX_DDC_SEGMENT 0x14

#define REG_HDMITX_DDC_REQUEST 0x15
#define CMD_DDC_CLEAR_FIFO 0x09
#define CMD_DDC_EDID_READ 0x03
#define CMD_DDC_BURST_READ 0x00

#define REG_HDMITX_DDC_STATUS 0x16
#define REG_HDMITX_DDC_FIFO   0x17

#define REG_DCC_FIFO_ACCESS_MODE 0x1B
#define B_EN_NEW_FIFO_ACCESS (1<<7)


#define REG_HDMITX_HDCP_AN_GEN 0x1F
#define B_EN_AN_GEN		(1<<0)

#define REG_HDMITX_SIPROM_CONFIG 0x20
#define B_ENA_SIPROM	  (1<<7)
#define B_SIPROM_SEL      (1<<4)
#define B_SIPROM_SEL_HDCP (0<<4)
#define B_SIPROM_SEL_EXT  (1<<4)

#define REG_HDMITX_HDCP_CONFIG 0x20
#define B_ENA_SIPROM	(1<<7)
#define B_EN_HDCP12_SYNCDET (1<<2)
#define B_EN_HDCP11_FEATURE (1<<1)
#define B_EN_CPDESIRE	(1<<0)
#define REG_HDMITX_HDCP_START  0x21
#define B_HDCP_AUTH_START (1<<0)

#define REG_HDMITX_HDCP_START  0x21

#define REG_HDMITX_HDCP_KSVLIST_CHECK 0x22
#define M_KSVLIST_CHECK 0x03
#define B_KSVLIST_FAIL (1<<1)
#define B_KSVLIST_DONE (1<<0)

#define REG_HDMITX_AKSV00 0x23
#define REG_HDMITX_AKSV01 0x24
#define REG_HDMITX_AKSV02 0x25
#define REG_HDMITX_AKSV03 0x26
#define REG_HDMITX_AKSV04 0x27

#define REG_HDMITX_AN00 0x28
#define REG_HDMITX_AN01 0x29
#define REG_HDMITX_AN02 0x2A
#define REG_HDMITX_AN03 0x2B
#define REG_HDMITX_AN04 0x2C
#define REG_HDMITX_AN05 0x2D
#define REG_HDMITX_AN06 0x2E
#define REG_HDMITX_AN07 0x2F

#define REG_HDMITX_GENAN00 0x30
#define REG_HDMITX_GENAN01 0x31
#define REG_HDMITX_GENAN02 0x32
#define REG_HDMITX_GENAN03 0x33
#define REG_HDMITX_GENAN04 0x34
#define REG_HDMITX_GENAN05 0x35
#define REG_HDMITX_GENAN06 0x36
#define REG_HDMITX_GENAN07 0x37


#define REG_HDMITX_SIP_READBACK 0x24

#define REG_HDMITX_SIP_ADDR0 0x30
#define REG_HDMITX_SIP_ADDR1 0x31

#define REG_HDMITX_SIP_COMMAND 0x33
#define SIP_COMMAND_READ (1<<2)

#define REG_HDMITX_SIP_OPTION_SEL 0x36
#define B_SIP_MACRO_SEL (1<<0)

#define REG_HDMITX_SIPMISC 0x37
#define B_SIP_RESET   (1<<4)
#define B_SIP_POWRDM (1<<0)

#define REG_HDMITX_ARI_L 0x38
#define REG_HDMITX_ARI_H 0x39
#define REG_HDMITX_APJ 0x3A

#define REG_HDMITX_BKSV00 0x3B
#define REG_HDMITX_BKSV01 0x3C
#define REG_HDMITX_BKSV02 0x3D
#define REG_HDMITX_BKSV03 0x3E
#define REG_HDMITX_BKSV04 0x3F

#define REG_HDMITX_BRI_L 0x40
#define REG_HDMITX_BRI_H 0x41
#define REG_HDMITX_BPJ   0x42

#define REG_HDMITX_ARI 0x38

#define REG_HDMITX_BCAPS     0x43
#define REG_HDMITX_BSTATUS_L 0x44
#define REG_HDMITX_BSTATUS_H 0x45

#define REG_HDMITX_HDCP_STATUS 0x46
#define B_HDCP_AUTH_DONE 0x80


#define REG_HDMITX_HDCP_TEST 0x4B
#define M_HDCP_4B_OPTION (0x03<<4)
#define EN_RI_COMB_READ (1<<5)
#define EN_R0_COMB_READ (1<<4)

#define REG_HDMITX_HDCP_OPTION 0x4F
#define B_EN_HDCP_AUTO_MUTE   (1<<3)
#define B_EN_RIPJ_CHK2DONE    (1<<2)
#define B_EN_SYNCDET_FAIL2FAIL (1<<2)
#define B_EN_HDCP_AUTO_REAURH (1<<0)
#define M_HDCP_4F_OPTION      (0x0F)

#define REG_HDMITX_SHA_SEL    0x50
#define M_SHA_SEL 0x07
#define SEL_V0 0x00
#define SEL_V1 0x01
#define SEL_V2 0x02
#define SEL_V3 0x03
#define SEL_V4 0x04
#define SEL_Mi 0x05



#define REG_HDMITX_AFE_DRV_CONFIG 0x61
#define B_DRV_PWRDN (1<<5)
#define B_DEV_REST  (1<<4)

#define REG_HDMITX_AFE_XP_CONFIG  0x62
#define B_XP_GAIN    (1<<7)
#define B_XP_PWDNPLL (1<<6)
#define B_XP_ENI     (1<<5)
#define B_XP_ER0     (1<<4)
#define B_XP_RESET   (1<<3)
#define B_XP_PWDNI   (1<<2)


#define REG_HDMITX_DRV_ISW 0x63
#define M_DRV_ISW 0x3F


#define REG_HDMITX_RINGOSC_CONFIG  0x65
#define M_RINGOSC_SPEED 0x03
#define RINGOSC_NORMAL 0
#define RINGOSC_FAST    (1<<0)
#define RINGOSC_SLOW    (1<<1)

#define REG_HDMITX_PULLUP_SEL 0x66
#define M_5KUP_SEL  (0x07<<3)
#define M_10KUP_SEL (0x07<<0)
#define R_PULLUP10K 0
#define R_PULLUP5K  4
#define SET_R_PULLUP10K R_PULLUP10K
#define SET_R_PULLUP5K (R_PULLUP5K<<3)



#define REG_HDMITX_AFE_SPEED 0x68
#define B_EXT_REXT          (1<<6)
#define B_AFE_SPEED         (1<<4)
#define B_AFE_SPEED_LOW     (1<<4)
#define B_AFE_SPEED_HIGH    (0<<4)



#define REG_HDMITX_VID_IO_CONFIG 0x71

#define B_PLLBUF_AUTO_RST   (1<<4)


#define REG_HDMITX_CSC_CONFIG 0x72
#define B_COLOR_CLIP (1<<3)
#define M_CSC_SEL 0x03


#define REG_HDMITX_CSCTABLE_START 0x73
#define REG_HDMITX_RGB_BANK_LEVEL 0x75


#define REG_HDMITX_CSC_SIGNEDBIT 0x88
#define B_CSC_SIGNED    (1<<7)
#define B_Y_SIGNED      (1<<6)
#define B_CB_SIGNED     (1<<5)
#define B_CR_SIGNED     (1<<4)


#define REG_HDMITX_AVMUTE_CTRL 0xC1
#define B_EN_AVMUTE (1<<0)

#define REG_HDMITX_VIDEO_BLACK 0xC2
#define B_EN_VIDEO_BLACK	(1<<6)

#define REG_HDMITX_DIS_ENCRYTION 0xC4
#define B_ENCRYPTION_DISABLE (1<<0)



#define REG_HDMITX_PCLK_CNT  0xD7
#define B_ENA_CNT   (1<<7)
#define M_DIV_SEL   (7<<4)
#define M_CLK_CNT_H  (0x0F)

#define REG_HDMITX_CLKCNT_L 0xD8

#define REG_HDMITX_OUT_COLOR 0xE0
#define M_OUT_COLOR  0xC0

#define REG_HDMITX_PATH_CTRL 0xE0
#define B_EN_REPEAT_CTROL_PKT (1<<5)
#define B_EN_TX_GEN_CTROL_PKT (1<<4)
#define B_EN_RXDDC_TO_CBUS    (1<<3)
#define B_EN_VSI_READBACK     (1<<1)
#define B_EN_USER_CONFIG_VSI  (1<<0)

#define REG_HDMITX_3DINFO_PB04 0xE3

#define REG_HDMITX_AVI_COLOR 0xE4

#define REG_HDMITX_MHL_DATA_PATH 0xE5
#define B_EN_AUTO_TXSRC (1<<7)
#define M_MHL_DATA_PATH (3<<4)
#define MHL_BYPASS_HDCP  (0<<4)
#define MHL_NO_HDCP      (2<<4)
#define MHL_TX_CTRL_HDCP (2<<4)
#define M_DDC_PATH_CTRL  (3<<0)

#define REG_HDMITX_RB_MHL_DATA_PATH 0xE6

#define REG_HDMITX_CBUS_SMT  0xE8
#define B_ENA_CBUS_SMT       (1<<1)

#define REG_HDMITX_IDDQ_MODE 0xE8
#define B_EN_IO_IDDQ    (1<<6)
#define B_IDDQ_OSC_POWDN    (1<<5)

#define REG_HDMITX_SPECIAL_IO_CTRL 0xE9
#define B_EN_EXT_VBUS_DET   (1<<2)

#define REG_HDMITX_TEST_CTRL0 0xF3
#define B_CBUS_DRVING       (0x03<<4)
#define SET_CBUS_DRINING    (0x01<<4)

#define B_FORCE_VIDEO_STABLE (1<<1)


#define REG_HDMITX_TEST_CTRL1 0xF4
#define M_DDC_SPEED   (0x03<<2)
#define DDC75K  (0)
#define DDC125K (1)
#define DDC312K (2)
#define SET_DDC_SPEED (DDC75K<<2)

#define REG_HDMITX_REG_LOCK 0xF8
#define UNLOCK_PWD_0 0xC3
#define UNLOCK_PWD_1 0xA5
#define LOCK_PWD     0xFF

#define REG_HDMITX_MHL_I2C_EN 0x8D
#define B_EN_MHL_I2C (1<<0)



/* ///////////////////////// */
/* reg list mhl */
/* ///////////////////////// */
#define REG_MHL_OSCD_CTRL 0x01
#define B_CBUS_DEGLITCH (1<<7)
#define M_100MS_ADJ     (3<<2)
#define SET_100MS_ADJ   (3<<2)
#define B_EN_CALIBRATION (1<<0)

#define REG_MHL_10US_TIME_INTE 0x02
#define REG_MHL_10US_TIME_FLOAT 0x03

#define REG_MHL_INT_04 0x04
#define INT_CBUS_NOT_DETECT  (1<<7)
#define INT_CBUS_WRITE_BURST (1<<6)
#define INT_CBUS_WRITE_START (1<<5)
#define INT_CBUS_MSC_RX_MSG  (1<<4)
#define INT_CBUS_RX_PKT_FAIL (1<<3)
#define INT_CBUS_RX_PKT_DONE (1<<2)
#define INT_CBUS_TX_PKT_FAIL (1<<1)
#define INT_CBUS_TX_PKT_DONE (1<<0)

#define REG_MHL_INT_05 0x05
#define INT_CBUS_DDC_RPD_FAIL (1<<7)
#define INT_CBUS_DDC_RPD_DONE (1<<6)
#define INT_CBUS_DDC_REQ_FAIL (1<<5)
#define INT_CBUS_DDC_REQ_DONE (1<<4)
#define INT_CBUS_MSC_RPD_FAIL (1<<3)
#define INT_CBUS_MSC_RPD_DONE (1<<2)
#define INT_CBUS_MSC_REQ_FAIL (1<<1)
#define INT_CBUS_MSC_REQ_DONE (1<<0)

#define REG_MHL_INT_06 0x06
#define INT_VBUS_CHANGE         (1<<7)
#define INT_DEV_CAP_CHANGE      (1<<6)
#define INT_MHL_MUTE_CHANGE     (1<<5)
#define INT_MHL_PATHEN_CANGE    (1<<4)
#define INT_CBUS_DISCOVER_FAIL  (1<<3)
#define INT_CBUS_DISCOVER_DONE  (1<<2)
#define INT_CBUS_1K_DETECT_FAIL (1<<1)
#define INT_CBUS_1K_DETECT_DONE (1<<0)



#define REG_MHL_INT_04_MASK08 0x08
#define B_MASK_CBUS_DETECT_IRQ  (1<<7)
#define B_MASK_CBUS_WR_BURST    (1<<6)
#define B_MASK_CBUS_WR_START    (1<<5)
#define B_MASK_MSC_RX_MSG       (1<<4)
#define B_MASK_CBUS_RX_PKT_FAIL (1<<3)
#define B_MASK_CBUS_RX_PKT_DONE (1<<2)
#define B_MASK_CBUS_TX_PKT_FAIL (1<<1)
#define B_MASK_CBUS_TX_PKT_DONE (1<<0)



#define REG_MHL_INT_05_MASK09 0x09
#define B_MASK_DDC_CMD_FAIL (1<<5)
#define B_MASK_DDC_CMD_DONE (1<<4)
#define B_MASK_MSC_REQ_DONE (1<<0)

#define REG_MHL_INT_06_MASK0A 0x0A
#define B_MASK_1K_FAIL_IRQ (1<<1)

#define REG_MHL_PATHEN_CTROL 0x0C
#define B_PACKET_PIXEL_HDCP_OPT (1<<7)
#define B_CLR_PATHEN            (1<<2)
#define B_SET_PATHEN            (1<<1)
#define B_HW_AUTO_PATH_EN       (1<<0)

#define REG_MHL_OPTION  0x0E
#define B_CBUS_LOW_WAKE_UP     (1<<4)


#define REG_MHL_CONTROL 0x0F
#define B_PACKET_PIXEL_GB_SWAP (1<<5)
#define B_DISABLE_MHL       (1<<4)
#define B_PACKET_PIXEL_MODE (1<<3)
#define B_EN_VBUS_OUT       (1<<1)
#define B_USB_SWITCH_ON        (1<<0)

#define REG_MHL_STATUS 0x10
#define B_STATUS_PATHEN     (1<<4)
#define B_STATUS_VBUS5V     (1<<3)
#define B_STATUS_1K_DETECT  (1<<2)
#define B_STATUS_EN_VBUS    (1<<1)
#define B_STATUS_CBUS_CONNECT (1<<0)

#define REG_R100MS_CNT0 0x12
#define REG_R100MS_CNT1 0x13
#define REG_R100MS_CNT2 0x14

#define REG_CBUS_PKT_FAIL_STATUS 0x15
#define REG_CBUS_DDC_FAIL_STATUS 0x16
#define REG_MSC_REQ_FAIL_STATUS_L 0x18
#define REG_MSC_REQ_FAIL_STATUS_H 0x19
#define REG_MSC_RPD_FAIL_STATUS_L 0x1A
#define REG_MSC_RPD_FAIL_STATUS_H 0x1B

#define REG_MHL_CBUS_STATUS 0x1C

#define REG_CBUS_DISCOVER_CONTROL0 0x28
#define REG_CBUS_DISCOVER_CONTROL1 0x29
#define M_CBUS_DISC_OPT_1 0x0E
#define SET_CBUS_DISC_OPT_1 0x04

#define REG_CBUS_LINK_LAYER_CTEL_36 0x36
#define M_T_ACK_HIGH    (0x0F<<4)
#define SET_T_ACK_HIGH  (0x0B<<4)
#define M_T_ACK_LOW     (0x03<<2)
#define SET_T_ACK_LOW   (0x01<<2)

#define REG_CBUS_DDC_CONTROL  0x38
#define B_EN_DDC_WAIT_ABORT 1

#define REG_MSC_TX_CMD_L 0x50
#define REG_MSC_TX_CMD_H 0x51

#define REG_MSC_HW_MASK_L 0x52

#define REG_MSC_HW_MASK_H 0x53

#define REG_MHL_MSC_TX_DATA0 0x54
#define REG_MHL_MSC_TX_DATA1 0x55
#define REG_MHL_MSC_RXDATA   0x56

#define REG_MSC_FIFO_MODE 0x5C
#define B_PKT_FIFO_BURST_MODE (1<<7)
#define B_MSC_ID_BURST      (1<<6)
#define B_MSC_BURST_OPT     (1<<5)
#define B_MSC_WRITE_BURST_MODE (1<<4)
#define B_MSC_HE_RETRY       (1<<3)
#define B_MSC_UCP_NACK_OPT   (1<<2)

#define REG_MHL_MSGRX_BYTE0 0x60
#define REG_MHL_MSGRX_BYTE1 0x61



#define REG_MHL_DEVICE_CAP_00 0x80
#define REG_MHL_DEVICE_CAP_01 0x81
#define REG_MHL_DEVICE_CAP_02 0x82
#define REG_MHL_DEVICE_CAP_03 0x83
#define REG_MHL_DEVICE_CAP_04 0x84
#define REG_MHL_DEVICE_CAP_05 0x85
#define REG_MHL_DEVICE_CAP_06 0x86
#define REG_MHL_DEVICE_CAP_07 0x87
#define REG_MHL_DEVICE_CAP_08 0x88
#define REG_MHL_DEVICE_CAP_09 0x89
#define REG_MHL_DEVICE_CAP_10 0x8A
#define REG_MHL_DEVICE_CAP_11 0x8B
#define REG_MHL_DEVICE_CAP_12 0x8C
#define REG_MHL_DEVICE_CAP_13 0x8D
#define REG_MHL_DEVICE_CAP_14 0x8E
#define REG_MHL_DEVICE_CAP_15 0x8F

#define REG_MHL_INTR_0 0xA0
#define REG_MHL_INTR_1 0xA1
#define REG_MHL_INTR_2 0xA2
#define REG_MHL_INTR_3 0XA3

#define REG_MHL_STATUS_00 0xB0
#define REG_MHL_STATUS_01 0xB1

/* end reglist */


/* Device Capbaility Register address and offsets have same values now.
 * For ex- while receving SET_INT, use these Registers [0x21...0x23],
 * while sending SET_INT, use these Offset[0x21...0x23]. */

/* 0xC8: CBUS Device Capability Registers */
#define MHL_VER_MAJOR			(1<<1)	/* Most Significant 4-bits */
#define MHL_VER_MINOR			(1<<0)	/* Least significant 4-bits */

#define DEV_TYPE_SOURCE			0x02
#define DEV_TYPE_SINK			0x01
#define DEV_TYPE_DONGLE			0x03
/* set when Device can supply power across VBUS,else clear this bit */
#define DEV_POW_SUPPLY			(1<<4)

#define VID_LINK_SUPP_RGB444		(1<<0)
#define VID_LINK_SUPP_YCBCR444		(1<<1)
#define VID_LINK_SUPP_YCBCR422		(1<<2)
#define VID_LINK_SUPP_PPIXEL		(1<<3)
#define VID_LINK_SUPP_ISLANDS		(1<<4)
#define VID_LINK_SUPP_VGA			(1<<5)

#define CBUS_AUD_LINK_MODE_REG	0x06
#define DEV_AUDIO_LINK_2CH		(1<<0)
#define DEV_AUDIO_LINK_8CH		(1<<1)

#define VIDEO_TYPE_GRAPHICS		(1<<0)
#define VIDEO_TYPE_PHOTO		(1<<1)
#define VIDEO_TYPE_CINEMA		(1<<2)
#define VIDEO_TYPE_GAME			(1<<3)
/* When cleared,ignore the video type */
#define SUPPORT_VT				(1<<7)

#define LOG_DEV_DISPLAY			(1<<0)
#define LOG_DEV_VIDEO			(1<<1)
#define LOG_DEV_AUDIO			(1<<2)
#define LOG_DEV_MEDIA			(1<<3)
#define LOG_DEV_TUNER			(1<<4)
#define LOG_DEV_RECORD			(1<<5)
#define LOG_DEV_SPEAKER			(1<<6)
#define LOG_DEV_GUI				(1<<7)

#define CBUS_FEATURE_FLAG_REG		0x0A
#define RCP_SUPPORT			(1<<0)
#define RAP_SUPPORT			(1<<1)
#define	SP_SUPPORT			(1<<2)
#define UCP_SEND            (1<<3)
#define UCP_RECIVE          (1<<4)


/* Bits [4...7] */
#define STATUS_SIZE			0x3
/* Bits [0...3] */
#define INTR_SIZE			0x3

#define DEVCAP_COUNT_MAX		16

#define MHL_DEVCAP_DEVSTATE		0x0
#define MHL_DEVCAP_MHL_VERSION		0x1
#define MHL_DEVCAP_DEV_CAT		0x2
#define MHL_DEVCAP_ADOPTER_ID_H		0x3
#define MHL_DEVCAP_ADOPTER_ID_L		0x4
#define MHL_DEVCAP_VID_LINK_MODE	0x5
#define MHL_DEVCAP_AUD_LINK_MODE	0x6
#define MHL_DEVCAP_VIDEO_TYPE		0x7
#define MHL_DEVCAP_LOG_DEV_MAP		0x8
#define MHL_DEVCAP_BANDWIDTH		0x9
#define MHL_DEVCAP_FEATURE_FLAG		0xa
#define MHL_DEVCAP_DEVICE_ID_H		0xb
#define MHL_DEVCAP_DEVICE_ID_L		0xc
#define MHL_DEVCAP_SCRATHPAD_SIZE	0xd
#define MHL_DEVCAP_INT_STAT_SIZE	0xe
#define MHL_DEVCAP_RESERVED		    0xf

/* Device interrupt register offset of connected device */
#define CBUS_MHL_INTR_REG_0		0x20
#define MHL_INT_DCAP_CHG		(1<<0)
#define	MHL_INT_DSCR_CHG		(1<<1)
#define MHL_INT_REQ_WRT			(1<<2)
#define MHL_INT_GRT_WRT			(1<<3)
#define MHL_INT_3D_REQ			(1<<4)

#define CBUS_MHL_INTR_REG_1		0x21
#define MHL_INT_EDID_CHG		(1<<1)

#define CBUS_MHL_INTR_REG_2		0x22
#define CBUS_MHL_INTR_REG_3		0x23


/* CBUS: Mask Registers for Interrupt offset registers(0x21...0x23) */
#define CBUS_MHL_INTR_REG_0_MASK	0x80
#define CBUS_MHL_INTR_REG_1_MASK	0x81
#define CBUS_MHL_INTR_REG_2_MASK	0x82
#define CBUS_MHL_INTR_REG_3_MASK	0x83

/* CBUS: Status Register offset for connected device */
#define CBUS_MHL_STATUS_OFFSET_0	0x30	/* CONNECTED_RDY */
#define MHL_STATUS_DCAP_READY		(1<<0)

#define CBUS_MHL_STATUS_OFFSET_1	0x31	/* LINK_MODE */
#define MHL_STATUS_CLK_PACKED_PIXEL	(1<<1)
#define MHL_STATUS_CLK_NORMAL		((1<<0) | (1<<1))
#define MHL_STATUS_PATH_ENABLED		(1<<3)
#define MHL_STATUS_MUTED			(1<<4)
#define MHL_STATUS_PATH_DISABLED	0x00

#define CBUS_MHL_STATUS_OFFSET_2	0x32
#define CBUS_MHL_STATUS_OFFSET_3	0x33

/* CBUS: Scratchpad Registers */
#define CBUS_MHL_SCRPAD_REG_0		0x40
#define CBUS_MHL_SCRPAD_REG_1		0x41
#define CBUS_MHL_SCRPAD_REG_2		0x42
#define CBUS_MHL_SCRPAD_REG_3		0x43
#define CBUS_MHL_SCRPAD_REG_4		0x44
#define CBUS_MHL_SCRPAD_REG_5		0x45
#define CBUS_MHL_SCRPAD_REG_6		0x46
#define CBUS_MHL_SCRPAD_REG_7		0x47
#define CBUS_MHL_SCRPAD_REG_8		0x48
#define CBUS_MHL_SCRPAD_REG_9		0x49
#define CBUS_MHL_SCRPAD_REG_A		0x4A
#define CBUS_MHL_SCRPAD_REG_B		0x4B
#define CBUS_MHL_SCRPAD_REG_C		0x4C
#define CBUS_MHL_SCRPAD_REG_D		0x4D
#define CBUS_MHL_SCRPAD_REG_E		0x4E
#define CBUS_MHL_SCRPAD_REG_F		0x4F


#define CBUS_CONN_STATUS_REG		0x91
#define DOWNSTREAM_HPD_MASK			0x04
#define DOWNSTREAM_CLR_HPD_RECD		0
#define DOWNSTREAM_SET_HPD_RECD		1

#define CBUS_MSC_INTR_REG		0x92
#define CBUS_MSC_INTR_ENABLE_REG	0x93
#define MSC_MSG_NACK_RCVD		(1<<7)
#define MSC_SET_INT_RCVD		(1<<6)
#define MSC_WRITE_BURST_RCVD	(1<<5)
#define MSC_MSG_RCVD			(1<<4)
#define MSC_WRITE_STAT_RCVD		(1<<3)
#define MSC_HPD_RCVD			(1<<2)
#define MSC_CMD_DONE			(1<<1)


#define CBUS_MSC_CMD_START_REG		0xB8
#define START_WRITE_BURST		(1<<4)
/*start WRITE_STAT or SET_INT */
#define START_WRITE_STAT_SET_INT	(1<<3)
#define START_READ_DEVCAP		(1<<2)
#define START_MSC_MSG			(1<<1)
#define	START_MISC_CMD			(1<<0)

/* sii8240 MSC error interrupt mask register 0xC8:0x95*/
/* Responder aborted DDC command at translation layer */
#define BIT_CBUS_CEC_ABRT              0x02
/* Responder sent a VS_MSG packet (response data or command.) */
#define BIT_CBUS_DDC_ABRT              0x04
#define BIT_CBUS_MSC_ABORT_RCVD        0x08
#define BIT_CBUS_MSC_SET_CAP_ID_RCVD   0x10
#define BIT_CBUS_RCV_VALID             0x20
#define BIT_CBUS_CMD_ABORT             0x40
#define BIT_CBUS_MHL_CABLE_CNX_CHG     0x80

/* MSC Command Opcode or offset for MSC commands.
 * if START_MISC_CMD, used as command Opcode,else used as offset[except
 * in case of MSC_MSG]
 */
#define MSC_CMD_OR_OFFSET_REG		0xB9

#define MSC_SEND_DATA1_REG		0xBA
#define MSC_SEND_DATA2_REG		0xBB
#define MSC_RCVD_DATA1_REG		0xBC
#define MSC_MSG_RCVD_DATA1_REG		0xBF
#define MSC_MSG_RCVD_DATA2_REG		0xC0

#define MSC_RETRY_INTERVAL_REG		0xD5	/* default:0x14 */

#define CBUS_MSC_ERROR_INTR_REG		0x94
#define CBUS_MSC_ERROR_INTR_ENABLE_REG	0x95

/* sii8240 initiated an MSC command,but an error occured;exact error
 * available in CBUS:0x9A */
#define SEND_ERROR_INT			(1<<6)

/* sii8240 was receiving an MSC command,but an error occured;exact error
 * available in CBUS:0x9C */
#define RECD_ERROR_INT			(1<<3)
#define RECD_DDC_ABORT_INT		(1<<2)


#define MSC_SEND_ERROR_REG		0x9A
#define MSC_RECVD_ERROR_REG		0x9C
/* [Sender]:Last command sent was ABORT-ed;[Receiver]:Received an ABORT */
#define CMD_ABORT			(1<<7)

/* [Sender]:Last command rejected because of undefined opcode,never sent over
 * CBUS;[Receiver]:first packet contained an undefined opcode */
#define CMD_UNDEF_OPCODE		(1<<3)

/* [Sender]:Failed to receive a response from Receiver[Timeout];
 * [Receiver]:Failed to receieve sender's response(Sender Timeout) */
#define CMD_TIMEOUT			(1<<2)

/*[Sender]: Last packet of MSC command received Protocol error
 *[Receiver]: Last packet of MSC command response received protocol error
 */
#define CMD_RCVD_PROT_ERR		(1<<1)
/* Retry Threshold exceeded */
#define CMD_MAX_FAIL		(1<<0)

/* events to be used by notifier;issued by board file or connector driver
   to notify MHL driver of various events */
#define DONGLE_DETACHED			0x00
#define DONGLE_ATTACHED			0x01
#define DONGLE_POWER_ATTACHED		0x02
#define DONGLE_POWER_DETACHED		0x03

#define MHL_CON_UNHANDLED		0
#define MHL_CON_HANDLED			1
#define MHL_CON_RETRY			2

/* Some Time limits as per MHL Specifications */
#define T_WAIT_TIMEOUT_RGND_INT		10000
#define T_WAIT_TIMEOUT_DISC_INT		8000

/* Host Device capabilities values */
#define DEV_STATE			0
/* MHL Version 1.2 */
#define DEV_MHL_VERSION			((MHL_VER_MAJOR<<4) | MHL_VER_MINOR)
/* A source kind of device which will not provide power across VBUS */
#define DEV_CAT_SOURCE_NO_PWR	(DEV_POW_SUPPLY | DEV_TYPE_SOURCE)
#define DEV_ADOPTER_ID_H		0x02
#define DEV_ADOPTER_ID_L		0x45
#define DEV_VID_LINK_MODE		0x3F
#define DEV_AUDIO_LINK_MODE		DEV_AUDIO_LINK_2CH
#define DEV_VIDEO_TYPE			0
#define DEV_LOGICAL_DEV			LOG_DEV_GUI
/*TODO: need to check bandwidth value whether 75MHz or 150 MHz */
#define DEV_BANDWIDTH			0xF
#define DEV_FEATURE_FLAG		(RCP_SUPPORT | RAP_SUPPORT | SP_SUPPORT | UCP_RECIVE | UCP_SEND)
#define DEV_DEVICE_ID_H			0x66	/*Samsung Device Specific values */
#define DEV_DEVICE_ID_L			0x82
#define DEV_SCRATCHPAD_SIZE		16
#define DEV_INT_STATUS_SIZE		((STATUS_SIZE<<4) | INTR_SIZE)
#define DEV_RESERVED			0


/* MHL DEVCAPS Video link mode*/
#define MHL_DEV_VID_LINK_SUPPRGB444         0x01
#define MHL_DEV_VID_LINK_SUPPYCBCR444       0x02
#define MHL_DEV_VID_LINK_SUPPYCBCR422       0x04
#define MHL_DEV_VID_LINK_SUPP_PPIXEL        0x08
#define MHL_DEV_VID_LINK_SUPP_ISLANDS       0x10

/* Device interrupt register offset of connected device */
#define CBUS_MHL_INTR_REG_0     0x20
#define MHL_INT_DCAP_CHG        (1<<0)
#define MHL_INT_DSCR_CHG        (1<<1)
#define MHL_INT_REQ_WRT         (1<<2)
#define MHL_INT_GRT_WRT         (1<<3)
#define MHL_INT_3D_REQ          (1<<4)

#define CBUS_MHL_INTR_REG_1     0x21
#define MHL_INT_EDID_CHG        (1<<1)

#define CBUS_MHL_INTR_REG_2     0x22
#define CBUS_MHL_INTR_REG_3     0x23



/* CBUS: Status Register offset for connected device */
#define CBUS_MHL_STATUS_OFFSET_0    0x30	/* CONNECTED_RDY */
#define MHL_STATUS_DCAP_READY       (1<<0)
#define CBUS_MHL_STATUS_OFFSET_1    0x31	/* LINK_MODE */
#define MHL_STATUS_CLK_PACKED_PIXEL (1<<1)
#define MHL_STATUS_CLK_NORMAL       ((1<<0) | (1<<1))
#define MHL_STATUS_PATH_ENABLED     (1<<3)
#define MHL_STATUS_MUTED            (1<<4)
#define MHL_STATUS_PATH_DISABLED    0x00

#define CBUS_MHL_STATUS_OFFSET_2    0x32
#define CBUS_MHL_STATUS_OFFSET_3    0x33


/* /////////////////////////////////////// */
/* Cbus command fire wait time */
/* Maxmun time for determin CBUS fail */
/* CBUSWAITTIME(ms) x CBUSWAITNUM */
/* CTS ASK 2500ms timeout */
/* /////////////////////////////////////// */
#define CBUSWAITTIME    1
#define CBUSWAITNUM     2500

/* /////////////////////////////////////// */
/* CBUS discover error Counter */
/*  */
/* determine Cbus connection by interrupt count */
/*  */
/* ////////////////////////////////////// */
#define CBUS_NO_5V_1k_TIMEOUT      3
#define CBUS_DISCOVER_TIMEOUT     20	/* timeout about  CBUS_DISCOVER_TIMEOUT*100ms */
#define CBUS_COMM_FAIL_TIMEOUT     2000

/* initial define options */
/* //////////////////////////////////////// */
/* CBUS Input Option */
/*  */
/* CBUS Discovery and Disconnect Option */
/* / */
/* /////////////////////////////////////// */

/* #define MHL_CBUS_FAIL_AUTO_RESTART */
/* #define MHL_IDDQ_LOW_POWER */
/* #define MHL_SUPPORT_POWER_OUT */
#define MHL_AUTO_TURN_BACK

/* //////////////////////////////////////// */
/* MSC Option */
/*  */
/*  */
/* / */
/* /////////////////////////////////////// */
#define MHL_OCLK_CAL_TIME      100	/* ms */


/* //////////////////////////////////////// */
/* HDCP setting */
/*  */
/*  */
/* /////////////////////////////////////// */
/* count about HDCP fail retry */
/* when HDCP timeout, video out blank screan */
/* NOTE: HDCP CTS require continue retry HDCP,if it fail. */
#define HDCP_FAIL_TIMEOUT	100
#define KSV_LIST_CHK_TIMEOUT  500

/* //////////////////////////////////////// */
/* Video color space convert */
/*  */
/*  */
/* //////////////////////////////////////// */
#define SUPPORT_INPUTRGB
#define SUPPORT_INPUTYUV444
#define SUPPORT_INPUTYUV422
#if defined(SUPPORT_INPUTYUV422) || defined(SUPPORT_INPUTYUV444)
#define SUPPORT_INPUTYUV
#endif

#define B_HDMITX_CSC_BYPASS    0
#define B_HDMITX_CSC_RGB2YUV   2
#define B_HDMITX_CSC_YUV2RGB   3


#define F_VIDMODE_ITU709  (1<<4)
#define F_VIDMODE_ITU601  0

#define F_VIDMODE_0_255   0
#define F_VIDMODE_16_235  (1<<5)


#define CSC_MATRIX_LEN 21

/* /////////////////////////////////////////////////////////////////// */
/* Packet and Info Frame definition and datastructure. */
/* /////////////////////////////////////////////////////////////////// */

#define VENDORSPEC_INFOFRAME_TYPE 0x81

#define SPD_INFOFRAME_TYPE 0x83
#define AUDIO_INFOFRAME_TYPE 0x84
#define MPEG_INFOFRAME_TYPE 0x85

#define VENDORSPEC_INFOFRAME_VER 0x01

#define SPD_INFOFRAME_VER 0x01
#define AUDIO_INFOFRAME_VER 0x01
#define MPEG_INFOFRAME_VER 0x01

#define VENDORSPEC_INFOFRAME_LEN 8

#define SPD_INFOFRAME_LEN 25
#define AUDIO_INFOFRAME_LEN 10
#define MPEG_INFOFRAME_LEN 10

#define ACP_PKT_LEN 9
#define ISRC1_PKT_LEN 16
#define ISRC2_PKT_LEN 16

#define IT668X_EDID_MAX_BLOCK 4
#define IT668X_EDID_BUF_LEN (128*IT668X_EDID_MAX_BLOCK)

/* /////////////////////////////////////////////////// */
struct IT668x_REG_INI {
	unsigned char ucAddr;
	unsigned char andmask;
	unsigned char ucValue;
};

enum mhl_state {
	MHL_USB_PWRDN = 0,
	MHL_USB,
	MHL_LINK_DISCONNECT,
	MHL_CBUS_START,
	/* MHL_1K_DETECT, */
	/* MHL_CBUS_DISCOVER, */
	MHL_CBUS_CONNECTED,
	MHL_Unknown
};

enum hdmi_video_state {
	HDMI_VIDEO_REST = 0,
	HDMI_VIDEO_WAIT,
	HDMI_VIDEO_ON,
	HDMI_VIDEO_Unknown
};


enum hdcp_state {
	HDCP_OFF = 0,
	HDCP_CP_START,
	HDCP_CP_GOING,
	HDCP_CP_DONE,
	HDCP_CP_FAIL,
};



enum cbus_command {
	IDLE = 0x00,
	MSC_EOF = 0x32,
	CBUS_ACK = 0x33,
	CBUS_NACK = 0x34,
	CBUS_ABORT = 0x35,
	WRITE_STAT = 0x60,
	SET_INT = 0x60,
	READ_DEVCAP = 0x61,
	GET_STATE = 0x62,
	GET_VENDOR_ID = 0x63,
	SET_HPD = 0x64,
	CLR_HPD = 0x65,
	SET_CAP_ID = 0x66,
	GET_CAP_ID = 0x67,
	MSC_MSG = 0x68,
	GET_SC1_ERR_CODE = 0x69,
	GET_DDC_ERR_CODE = 0x6A,
	GET_MSC_ERR_CODE = 0x6B,
	WRITE_BURST = 0x6C,
	GET_SC3_ERR_CODE = 0x6D,
};
enum msc_subcommand {
	/* MSC_MSG Sub-Command codes */
	MSG_MSGE = 0x02,
	MSG_RCP = 0x10,
	MSG_RCPK = 0x11,
	MSG_RCPE = 0x12,
	MSG_RAP = 0x20,
	MSG_RAPK = 0x21,
	MSG_UCP = 0x30,
	MSG_UCPK = 0x31,
	MSG_UCPE = 0x32,

	MSG_MOUSE = 0x40 | 0x80,
	MSG_MOUSEK = 0x41 | 0x80,
	MSG_MOUSEE = 0x42 | 0x80,

	MSG_KB = 0x50 | 0x80,
	MSG_KBK = 0x51 | 0x80,
	MSG_KBE = 0x52 | 0x80,

};


enum mhl_cbus_cmd_reg {

	FIRE_WRITE_STAT_INT = 0x0080,	/* mhl register 0x50 */
	FIRE_READ_DEVCAP = 0x0040,
	FIRE_GET_MSC_ERRORCODE = 0x0020,
	FIRE_GET_DDC_ERRORCODE = 0x0010,
	FIRE_CLR_HPD = 0x0008,
	FIRE_SET_HPD = 0x0004,
	FIRE_GET_VENDOR_ID = 0x0002,
	FIRE_GET_STATE = 0x0001,

	FIRE_FW_MODE = 0x8000,	/* mhl register 0x51 */
	FIRE_MSC_MSG = 0x0200,
	FIRE_WRITE_BURST = 0x0100,

};

/* /////////////////////////////////////// */
/*  */
/* NOTE: all the info struct in infoframe */
/* is aligned from LSB. */
/*  */
/* ////////////////////////////////////// */
#define AVI_INFOFRAME_TYPE 0x82
#define AVI_INFOFRAME_VER 0x02
#define AVI_INFOFRAME_LEN 13
/* |Type */
/* |Ver */
/* |Len */
/* |Scan:2 | BarInfo:2 | ActiveFmtInfoPresent:1 | ColorMode:2 | FU1:1| */
/* |AspectRatio:4 | ictureAspectRatio:2 | Colorimetry:2 */
/* |Scaling:2 | FU2:6 */
/* |VIC:7 | FU3:1 */
/* |PixelRepetition:4 | FU4:4 */
/* |Ln_End_Top */
/* |Ln_Start_Bottom */
/* |Pix_End_Left */
/* |Pix_Start_Right */

struct avi_infoframe {

	unsigned char avi_hb[3];
	unsigned char avi_db[AVI_INFOFRAME_LEN];

};


enum avi_cmd_type {
	AVI_CMD_NONE = 0x00,
	HPD_HIGH_EVENT = 0x01,
	HPD_LOW_EVENT,
	AVI_CDREST,
	CEA_NO_AVI,
	CEA_NEW_AVI,
	AVI_HDCP_RESTART,
	CHIP_RESET,
	AVI_CMD_REVERSE,
};


enum hdcp_cmd_type {
	HDCP_CMD_NONE = 0x00,
	HDCP_START,
	HDCP_KSV_CHK,
	HDCP_CMD_REVERSE,
};
enum it668x_color_scale {

	CSC_NONE = 0x00,

	CSC_RGB2YUV_ITU601_0_255 = 0x02,
	CSC_RGB2YUV_ITU709_0_255 = 0x12,
	CSC_RGB2YUV_ITU601_16_235 = 0x22,
	CSC_RGB2YUV_ITU709_16_235 = 0x32,

	CSC_YUV2RGB_ITU601_0_255 = 0x03,
	CSC_YUV2RGB_ITU709_0_255 = 0x13,
	CSC_YUV2RGB_ITU601_16_235 = 0x23,
	CSC_YUV2RGB_ITU709_16_235 = 0x33,
};

struct cbus_data {
	enum cbus_command cmd;	/* cbus command type */
	u8 offset;		/* for MSC_MSG,stores msc_subcommand */
	u8 data;
	struct completion complete;
	bool use_completion;
	struct list_head list;
};

struct timing_table {
	int h_active;
	int v_active;
	int htotal;
	int vtotal;
	int pclk;
	int h_frontporch;
	int h_sync_with;
	int h_backporch;
	int v_frontporch;
	int v_sync_with;
	int v_backporch;
	int scanmode;
	int vpolarity;
	int hpolarity;
	int pixel_repeat;
	char *format;
	int vic;
};


struct it668x_data {

	unsigned int ver;
	unsigned int tx_ver;
	unsigned long rclk;
	unsigned int oscdiv;
	unsigned char rclk_div;
	unsigned char mhl_pow_support;
	unsigned char mhl_pow_en;
	unsigned char hdmi_hpd_rxsen;
	bool sink_support_hdmi;

	unsigned device_powerdown;

	enum hdmi_video_state video_state;

	/* unsigned int input_vic; */
	unsigned char input_color_mode;
	unsigned char output_color_mode;
	unsigned char color_dyn_range;
	unsigned char color_ycbcr_coef;
	unsigned char pixel_repeat;

	unsigned char force_color_mode;
	unsigned char recive_hdmi_mode;

	unsigned char sink_support_packet_pixel_mode;
	unsigned char auto_packet_pixel_mode;

	/* struct avi_infoframe current_avi; */
	/* struct avi_infoframe new_avi; */
	/* 3D Option */
	unsigned char enable_3d;
	unsigned char sel_3dformat;


	/* Audio Option */
	/* unsigned char audio_enable ; */
	/* unsigned char audio_interface ; // I2S or SPDIF */
	/* unsigned char audio_freq ; //audio sampling freq */
	/* unsigned char audio_channel ; */
	/* unsigned char audio_type ; // LPCM, NLPCM, HBR, DSD */

	/* HDCP */
	unsigned int hdcp_enable;

	enum hdcp_state hdcp_state;

	unsigned int hdcp_fail_time_out;

	/* unsigned int hdcp_ri_timeoout ; */
	/* unsigned int hdcp_ksv_check_timeout ; */
	/* unsigned int hdcp_sync_detect_timeout; */
	unsigned int ddc_bypass;
	/* unsigned int ddc_wait_abort; */


	/* CBUS MSC */
	enum mhl_state mhl_state;

	unsigned int cbus_detect_timeout;
	unsigned int cbus_1k_det_timeout;
	unsigned int cbus_discover_timeout;	/* Discover fail count determine when to switch to USB mode */
	unsigned int cbus_packet_fail_counter;
	unsigned int cbus_msc_fail_counter;
	unsigned int cbus_ddc_fail_counter;

	unsigned char mhl_active_link_mode;
	unsigned char mhl_devcap[16];

	struct it668x_platform_data *pdata;
	unsigned char edidbuf[IT668X_EDID_BUF_LEN];


	unsigned int keycode[MHL_RCP_NUM_KEYS];

	int irq;
	struct mutex lock;

	/* * */
	/* mhl cbus event process variables */
	/* bool                                    cbus_ready; */
	unsigned char msc_err_abort;
	struct mutex cbus_lock;
	struct completion cbus_complete;
	/* struct work_struct          cbus_work; */
	struct work_struct msc_work;
	struct workqueue_struct *cbus_cmd_wqs;
	struct list_head cbus_data_list;

	struct mutex msc_lock;
	struct mutex ddc_lock;
	/* * */
	wait_queue_head_t it668x_wq;
	wait_queue_head_t avi_wq;
	wait_queue_head_t cbus_wq;
	wait_queue_head_t hdcp_wq;

	bool avi_work;
	enum avi_cmd_type avi_cmd;


	bool hdcp_work;
	enum hdcp_cmd_type hdcp_cmd;
	bool hdcp_abort;

	struct work_struct cbus_cmd_work;
	struct work_struct avi_control_work;
	struct work_struct hdcp_control_work;

	struct workqueue_struct *avi_cmd_wqs;


	struct completion ddc_complete;

	struct mutex input_lock;

	struct input_dev *rcp_input;

	int hdcp_work_around;

	struct class *sec_mhl;

	unsigned char ioctl_cmd_buffer[64];
	unsigned char ioctl_data_buffer[64];

	wait_queue_head_t power_resume_wq;
	wait_queue_head_t mcu_hold_wq;
	atomic_t power_down;
	atomic_t mcu_hold;
	atomic_t dump_register;
	atomic_t fw_reinit;

	bool switch_at_mhl;
	bool is_edid_ready;
	bool use_internal_edid_before_edid_ready;
	void *debugfs;

	struct device *it668x_dev;
};

#endif
