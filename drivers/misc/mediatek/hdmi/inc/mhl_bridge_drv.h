typedef enum {
    MHLTX_CONNECT_NO_DEVICE,
    HDMITX_CONNECT_ACTIVE,
} MHLTX_CONNECT_STATE;

typedef struct {
    int (*init)(void);
    int (*enter)(void);
    int (*exit)(void);
    void (*suspend)(void);
    void (*resume)(void);
    void  (*power_on)(void);
    void (*power_off)(void);
    MHLTX_CONNECT_STATE (*get_state)(void);
    void (*debug)(unsigned char *pcmdbuf);
    void (*enablehdcp)(unsigned char u1hdcponoff);
    int (*getedid)(unsigned char *pedidbuf);
    void (*mutehdmi)(unsigned char enable);
    unsigned char (*getppmodesupport)(void);
    void (*resolution)(unsigned char res, unsigned char cs);
} MHL_BRIDGE_DRIVER;

const MHL_BRIDGE_DRIVER *MHL_Bridge_GetDriver(void);
