#ifndef __M4U_PORT_PRIV_H__
#define __M4U_PORT_PRIV_H__

static const char* gM4U_SMILARB[] = {"mediatek,SMI_LARB0", "mediatek,SMI_LARB1"};

#define M4U0_PORT_INIT(slave, larb, port)  0,slave,larb,port,(((larb)<<7)|((port)<<2)), 1
 
m4u_port_t gM4uPort[] = 
{
    { "DISP_OVL0",                  M4U0_PORT_INIT( 0, 0,  0    ),    },
    { "DISP_RDMA0",                 M4U0_PORT_INIT( 0, 0,  1    ),    },
    { "DISP_WDMA0",                 M4U0_PORT_INIT( 0, 0,  2    ),    },
    { "MDP_RDMA",                   M4U0_PORT_INIT( 0, 0,  3    ),    },
    { "MDP_WDMA",                   M4U0_PORT_INIT( 0, 0,  4    ),    },
    { "MDP_WROT",                   M4U0_PORT_INIT( 0, 0,  5    ),    },    
                                                                    
    { "CAM_IMGO",                   M4U0_PORT_INIT( 0, 1,  0    ),    },
    { "CAM_IMG2O",                  M4U0_PORT_INIT( 0, 1,  1    ),    },    
    { "CAM_LSCI",                   M4U0_PORT_INIT( 0, 1,  2    ),    },
    { "VENC_BSDMA_VDEC_POST0",      M4U0_PORT_INIT( 0, 1,  3    ),    },    
    { "CAM_IMGI",                   M4U0_PORT_INIT( 0, 1,  4    ),    },
    { "CAM_ESFKO",                  M4U0_PORT_INIT( 0, 1,  5    ),    },    
    { "CAM_AAO",                    M4U0_PORT_INIT( 0, 1,  6    ),    },                                    
    { "VENC_MVQP",                  M4U0_PORT_INIT( 0, 1,  7    ),    },    
    { "VENC_MC",                    M4U0_PORT_INIT( 0, 1,  8    ),    },
    { "VENC_CDMA_VDEC_CDMA",        M4U0_PORT_INIT( 0, 1,  9    ),    },    
    { "VENC_REC_VDEC_WDMA",         M4U0_PORT_INIT( 0, 1,  10	),	  },	

};

#endif
