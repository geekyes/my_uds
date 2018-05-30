
/*
 * 使用的缩写：
 * p -- pointer
 * fp -- function pointer
 * ser -- service
 * rc -- response code
 * res -- result
 * req -- request
 * sec -- security
 * gen -- generate
 * calc -- calculate
 * lev -- level
 * args -- arguments
 * max -- maximum
 * mem -- memory
 * dev -- device
 * op -- operation
 * fmt -- format
 */

#include "uds.h"

#undef NULL
#define NULL ((void*)0)
#define LOW_HALF_OF_BYTE_MASK ((uint8_t)0x0f)
#define HIGH_HALF_OF_BYTE_MASK ((uint8_t)0x0f)
#define NR_SID ((uint8_t)0x7f)
#define PR_SID ((uint8_t)((p_pdu->SID) + 0x40))
/* 服务请求的PDU数据长度 */
#define SESSION_CONTROL_REQ_PDU_LEN ((uint32_t)2)
#define SECURITY_ACCESS_SEED_REQ_PDU_LEN ((uint32_t)2)
#define SECURITY_ACCESS_KEY_REQ_PDU_LEN ((uint32_t)4)
#define ROUTINE_CONTROL_REQ_PDU_LEN ((uint32_t)x)
#define REQUEST_DOWNLOAD_REQ_PDU_LEN ((uint32_t)11)
#define REQUEST_UPLOAD_REQ_PDU_LEN ((uint32_t)11)
#define TRANSFER_DATA_REQ_PDU_LEN ((uint32_t)2)
#define REQUEST_TRANSFER_EXIT_REQ_PDU_LEN ((uint32_t)2)
#define ECU_RESET_REQ_PDU_LEN ((uint32_t)2)

#define SEC_ACCESS_FAILED_CNT_MAX ((uint8_t)3)

#define TIMER(NAME) do {\
    if (session.NAME##_timer.is_valid) {\
        if (session.NAME##_timer.cnt) {\
            session.NAME##_timer.status = TIMER_TIMING;\
            session.NAME##_timer.cnt--;\
        }\
        else {\
            session.NAME##_timer.cnt = 0;\
            session.NAME##_timer.is_valid = TIMER_STOP;\
            session.NAME##_timer.status = TIMER_TIMEOUT;\
        }\
    }\
} while (0)
#define ENABLE_TIMER(NAME, CNT) do {\
    session.NAME##_timer.cnt = CNT;\
    session.NAME##_timer.status = TIMER_TIMING;\
    session.NAME##_timer.is_valid = TIMER_START;\
} while (0)
#define DISENABLE_TIMER(NAME) do {\
    session.NAME##_timer.cnt = 0;\
    session.NAME##_timer.status = TIMER_TIMEOUT;\
    session.NAME##_timer.is_valid = TIMER_STOP;\
} while (0)
#define GET_TIMER_STATUS(NAME)\
    (session.NAME##_timer.status)

#define RESET_LOAD() do {\
    session.loader.status = LOAD_IDLE;\
    session.loader.start_addr = 0;\
    session.loader.size = 0;\
    session.loader.block_number = 0;\
} while (0)

typedef enum {
    DEFAULT_SESSION = 0x1, PROGRAMING_SESSION, EXTENDED_SESSION 
} session_type_t;

typedef enum {
    PR = 0x00, /* positive response */
    SFNS = 0x12, /* sub-function not supportrd */
    IMLOIF = 0x13, /* incorrect(不正确) message length or invalid format */
    CNC = 0x22, /* conditions not correct */
    RSE = 0x24, /* request sequence error */
    ROOR = 0x31, /* request out of range */
    SAD = 0x33, /* security access denied(拒绝) */
    IK = 0x35, /* invalid key */
    ENOA = 0x36, /* exceeded(溢出的) number of attempts(尝试) */
    RTDNE = 0x37, /* required time delay not expired(过期) */
    UDNA = 0x70, /* upload download not accepted */
    TDS = 0x71, /* transfer data suspended */
    GPF = 0x72, /* general programming failure */
    WBSC = 0x73, /* wrong block sequence counter */
    VTH = 0x92, /* voltage too high */
    VTL = 0x93, /* voltage too low */
    UNRC = 0xff /* unkown NRC */
} nrc_t;

typedef struct {
    // TIMER_STOP: 停止定时器 TIMER_START: 开始定时
    enum {TIMER_STOP, TIMER_START} is_valid;
    // TIMER_TIMING: 定时中 TIMER_TIMEOUT: 定时时间到 
    enum {TIMER_TIMING, TIMER_TIMEOUT} status;
    uint16_t cnt;
}timer_t;

/* uds 的数据格式 */
typedef struct {
    uint8_t SID;
    uint8_t *p_ser_data; // 服务的数据
    uint32_t len; // ser_data的长度
} A_PDU_t;

/* 服务函数指针的的申明 */
typedef nrc_t (*fp_ser_t)(A_PDU_t *p_pdu);

typedef struct {
    A_PDU_t A_PDU;
    fp_ser_t p_func;
    nrc_t nrc;
    uint32_t positive_len; // 服务需要positive response 的数据长度
} ser_t;

typedef struct {
    enum {
        SESSION_CONTROL = 0x10,
        SECURITY_ACCESS = 0x27,
        ROUTINE_CONTROL = 0x31,
        REQUEST_DOWNLOAD = 0x34,
        REQUEST_UPLOAD = 0x35,
        TRANSFER_DATA = 0x36,
        REQUEST_TRANSFER_EXIT = 0x37,
        ECU_RESET = 0x11,
        UNKOWN_SERVICE = 0xff
    } SID;
    fp_ser_t p_func;
} ser_func_t;

typedef struct {
    enum {LOAD_IDLE, LOADING, LOAD_COMPLETED} status;
    enum {DOWNLOAD, UPLOAD} direction;
    addr_t start_addr;
    size_of_op_t size;
    mem_dev_t dev;
    uint8_t block_number;
} load_mem_t;

typedef struct {
    /* 会话(session)模式 */
    session_type_t type;
    /* tp接收来数据的缓冲区 */
    uint8_t tp_rcv_buf[N_DATA_MAX];
    /* 把接收到的数据解析(parse)为对应的服务 */
    ser_t ser;
    /* negative/positive response buffer */
    uint8_t tp_send_buf[N_DATA_MAX];
    /* 当前会话的安全等级 */
    enum {lev_1, lev_2} sec_lev;
    /* 禁止安全访问的定时器 */
    timer_t sec_timer;
    /* 数据上传或下载的加载器 */
    load_mem_t loader;
    /* 单片机复位支持 */
    reset_op_t mcu;
    /* TODO uds的时间参数 */
} session_t;

/* 解析从tp接收来的数据为uds数据格式 */
static void data_parse(ser_t *p_ser, uint32_t len);
/* 启动服务 */
static void start_ser(ser_t *p_ser);
/* negative/positive response */
static void response(nrc_t nrc);
/* 时间参数控制 */
static void timer_ctrl(void);

/* TODO 服务函数申明 start */
static nrc_t session_control(A_PDU_t *p_pdu); // 0x10
static nrc_t security_access(A_PDU_t *p_pdu); // 0x27
static nrc_t routine_control(A_PDU_t *p_pdu); // 0x31
static nrc_t request_download(A_PDU_t *p_pdu); // 0x34
static nrc_t request_upload(A_PDU_t *p_pdu); // 0x35
static nrc_t transfer_data(A_PDU_t *p_pdu); // 0x36
static nrc_t request_transfer_exit(A_PDU_t *p_pdu); // 0x37
static nrc_t ecu_reset(A_PDU_t *p_pdu); // 0x11
/* 服务函数申明 end */

/* 生成一个随机数 */
static uint16_t gen_seed(void);
/* 根据seed生成key */
static uint16_t calc_key(uint16_t seed);
/* 从p_pdu->p_ser_data中获取地址和操作区域 */
static void get_addr_and_size(const uint8_t *p_data, addr_t *addr,\
        size_of_op_t *op_size);
/* 将p_src的数数据拷贝到p_dest */
static void data_copy(uint8_t *p_dest, const uint8_t *p_src, uint8_t size);
/* 将p_data以字节填充为fill */
static void data_fill(uint8_t *p_data, uint8_t fill, uint8_t size);

/* 懒得写A_AI，使用网络层地址的定义 */
const N_AI_t rcv_addr = {.id = 0x777, .N_TAtype = PHYSICAL_ADDR};
const N_AI_t send_addr = {.id = 0x778, .N_TAtype = PHYSICAL_ADDR};
/* TODO 添加服务请增加表项并实现对应服务函数 */
static const ser_func_t ser_func_list[] = {
    {SESSION_CONTROL,       session_control},
    {SECURITY_ACCESS,       security_access},
    {ROUTINE_CONTROL,       routine_control},
    {REQUEST_DOWNLOAD,      request_download},
    {REQUEST_UPLOAD,        request_upload},
    {TRANSFER_DATA,         transfer_data},
    {REQUEST_TRANSFER_EXIT, request_transfer_exit},
    {ECU_RESET,             ecu_reset},
    {UNKOWN_SERVICE,        NULL}
};

static session_t session;

/* 加载储存和系统复位的函数 */
void init_uds(mem_dev_t dev, reset_op_t op)
{
    session.mcu = op;
    session.loader.dev = dev;
    /* init 禁止访问的定时器 */
    DISENABLE_TIMER(sec);
}

// 需要周期调用，主要是对会话时间的控制
void main_uds_polling(void)
{
    rcv_result_t rcv_res = receive_network_layer(rcv_addr,\
            session.tp_rcv_buf, N_DATA_MAX);

    /* 说明接收到数据需要解析 */
    if (rcv_res.N_result != N_OK)
    {
        // 接收错误处理
    }
    else if (rcv_res.length)
    {
        data_parse(&session.ser, rcv_res.length);
        /* 启动服务 */
        start_ser(&session.ser);
        /* negative/positive response */
        response(session.ser.nrc);
    }
    
    /*
     * TODO 检测发送结果，需要解决发送与结果的同步，
     * 原因是在下次发送会清除本次的发送结果。
     * 可能返回的值
     * N_INVALID
     * N_TIMEOUT_Bs
     * N_BUFFER_OVFLW
     * N_INVALID_FS
     * N_TIMEOUT_A
     * N_OK
     */
    /* N_result_t res = confirm_network_layer(); */

    /* 时间参数控制 */
    timer_ctrl();
}

/* 解析从tp接收来的数据为uds数据格式 */
static void data_parse(ser_t *p_ser, uint32_t len)
{
    if (NULL == p_ser && 0 == len)
    {
        return;
    }

    for (uint8_t i = 0;\
            i < sizeof(ser_func_list) / sizeof(ser_func_t);\
            i++)
    {
        if (session.tp_rcv_buf[0] == ser_func_list[i].SID)
        {
            p_ser->p_func = ser_func_list[i].p_func;
            p_ser->A_PDU.SID = session.tp_rcv_buf[0];
            p_ser->A_PDU.p_ser_data = &session.tp_rcv_buf[1];
            p_ser->A_PDU.len = len;
        }
    }
}

/* 启动服务 */
static void start_ser(ser_t *p_ser)
{
    if (NULL == p_ser && NULL == p_ser->p_func\
            && NULL == p_ser->A_PDU.p_ser_data\
            && 0 == p_ser->A_PDU.len)
    {
        return;
    }

    /* TODO 安全验证 */
    p_ser->nrc = p_ser->p_func(&p_ser->A_PDU);
}

/* negative/positive response */
static void response(nrc_t nrc)
{
    if (nrc) // negative response
    {
        session.tp_send_buf[0] = NR_SID;
        session.tp_send_buf[1] = session.ser.A_PDU.SID;
        session.tp_send_buf[2] = nrc;
        /* 3 -- 发送数据的个数，单位字节 */
        send_network_layer(send_addr, session.tp_send_buf, 3);
    }
    else if (session.ser.positive_len) // positive response
    {
        /* TODO 构建 positive response 的职责交给服务函数 */
        send_network_layer(send_addr,\
                session.tp_send_buf, session.ser.positive_len);
    }
}

/* 时间参数控制 */
static void timer_ctrl(void)
{
    TIMER(sec); // 禁止安全访问时间
}

/* 生成一个随机数 */
static uint16_t gen_seed(void)
{
    return 123;
}

/* 根据seed生成key */
static uint16_t calc_key(uint16_t seed)
{
    return seed | 0x1234;
}

/* TODO 地址和长度都使用32位宽
 * 从p_pdu->p_ser_data中获取地址和操作区域 */
static void get_addr_and_size(const uint8_t *p_data, addr_t *p_addr,\
        size_of_op_t *p_op_size)
{
    if (NULL == p_data && NULL == p_addr && NULL == p_op_size)
    {
        return;
    }
    else
    {
        uint8_t addr_fmt_indentifier = *(p_data + 0) & LOW_HALF_OF_BYTE_MASK;
        uint8_t size_fmt_indentifier = *(p_data + 0) & HIGH_HALF_OF_BYTE_MASK;
        uint8_t addr_buf[sizeof(addr_t)] = {0};
        uint8_t size_buf[sizeof(size_of_op_t)] = {0};
        
        if (addr_fmt_indentifier < sizeof(addr_t))
        {
            data_copy(addr_buf + (sizeof(addr_t) - addr_fmt_indentifier),\
                    p_data, addr_fmt_indentifier);
        }
        if (size_fmt_indentifier < sizeof(size_of_op_t))
        {
            data_copy(size_buf + (sizeof(size_of_op_t) - size_fmt_indentifier),\
                    p_data, size_fmt_indentifier);
        }   
        
        #ifdef BIG_ENDIAN
        *p_addr = (addr_t)addr_buf;
        *p_op_size = (size_of_op_t)size_buf;
        #else
        /* 大端到小端字节序 */
        for (uint8_t i = 0, j = sizeof(addr_t) - 1;\
                i < j; i++, j--)
        {
            addr_buf[i] ^= addr_buf[j];
            addr_buf[j] ^= addr_buf[i];
            addr_buf[i] ^= addr_buf[j];
        }   
        for (uint8_t i = 0, j = sizeof(size_of_op_t) - 1;\
                i < j; i++, j--)
        {
            size_buf[i] ^= size_buf[j];
            size_buf[j] ^= size_buf[i];
            size_buf[i] ^= size_buf[j];
        }
        *p_addr = (addr_t)addr_buf;
        *p_op_size = (size_of_op_t)size_buf;
        #endif
    }
}

// 将p_data以字节填充为fill
static void data_fill(uint8_t *p_data, uint8_t fill, uint8_t size)
{
    if (NULL == p_data) {
        return;
    }

    for (uint8_t i = 0; i < size; i++) {
        *p_data++ = fill;
    }
}

// 将src的数数据拷贝到dest
static void data_copy(uint8_t *p_dest, const uint8_t *p_src, uint8_t size)
{
    if (NULL == p_dest || NULL == p_src || size > CAN_FRAME_SIZE - 1) {
        // 记录错误
        return;
    }

    for (uint8_t i = 0; i < size; i++) {
            *p_dest++ = *p_src++;
    }
}

/* TODO 服务函数定义 start */
static nrc_t session_control(A_PDU_t *p_pdu) // 0x10
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else
    {
        session_type_t type = *(p_pdu->p_ser_data + 0);
        
        if (type > EXTENDED_SESSION)
        {
            ret = SFNS; // 不支持的子功能
        }
        else if (p_pdu->len != SESSION_CONTROL_REQ_PDU_LEN)
        {
            ret = IMLOIF; // 不正确的信息长度或无效格式
        }
        else if (session.loader.status != LOAD_IDLE)
        {
            ret = CNC; // 条件不正确
        }
        else
        {
            session.type = type;
            /* TODO 构建positive response  */
            session.tp_send_buf[0] = PR_SID;
            session.tp_send_buf[1] = (uint8_t)session.type;
            session.ser.positive_len = 2;
            ret = PR; // mark positive response
        }
    }

    return ret;
}

static nrc_t security_access(A_PDU_t *p_pdu) // 0x27
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else if (TIMER_TIMING == GET_TIMER_STATUS(sec)) // 禁止访问时间没到
    {
        ret = RTDNE;
    }
    else if (lev_1 == session.sec_lev)
    {
        static struct {
            enum {
                WAIT_CLIENT_SEED, SER_SEND_SEED, 
                WAIT_CLIENT_KEY, SER_VERIFY_KEY
            } status; // 安全认证过程的的状态
            uint16_t seed;
            uint8_t failed_cnt;
        } sec_access_args = {WAIT_CLIENT_SEED, 0, 0};
        enum {REQ_SEED = 0x1, SEND_KEY = 0x2} sec_access_type;
        
        sec_access_type = *(p_pdu->p_ser_data + 0);
        
        /* 关闭禁止访问的定时器 */
        DISENABLE_TIMER(sec);
        
        switch (sec_access_args.status)
        {
            case WAIT_CLIENT_SEED:
                if (SEND_KEY == sec_access_type)
                {
                    ret = RSE; // 在req_seed接收到send_key
                    break;
                }
                else if (sec_access_type != REQ_SEED)
                {
                    ret = SFNS;
                    break;
                }
                else if (p_pdu->len != SECURITY_ACCESS_SEED_REQ_PDU_LEN)
                {
                    ret = IMLOIF;
                    break;
                }
                else if (session.loader.status != LOAD_IDLE)
                {
                    ret = CNC; // 条件不正确
                    break;
                }
                else
                {
                    sec_access_args.status = SER_SEND_SEED;
                }
            case SER_SEND_SEED:
                /* TODO 构建 positive response */
                sec_access_args.seed = gen_seed();
                if (sec_access_args.seed)
                {
                    session.tp_send_buf[0] = PR_SID;
                    session.tp_send_buf[1] = (uint8_t)sec_access_type;
                    session.tp_send_buf[2] = (uint8_t)(sec_access_args.seed >> 8);
                    session.tp_send_buf[3] = (uint8_t)(sec_access_args.seed);
                    session.ser.positive_len = 4;
                    ret = PR;
                    sec_access_args.status = WAIT_CLIENT_KEY;
                }
                else // 万一种子生成失败，必须重新来过
                {
                    sec_access_args.status = WAIT_CLIENT_SEED;
                }
                break;
            case WAIT_CLIENT_KEY:
                if (REQ_SEED == sec_access_type)
                {
                    ret = RSE; // 在send_key接收到req_seed
                    sec_access_args.status = WAIT_CLIENT_SEED;
                    break;
                }
                else if (sec_access_type != SEND_KEY)
                {
                    ret = SFNS;
                    sec_access_args.status = WAIT_CLIENT_SEED;
                    break;
                }
                else if (p_pdu->len != SECURITY_ACCESS_KEY_REQ_PDU_LEN)
                {
                    ret = IMLOIF;
                    sec_access_args.status = WAIT_CLIENT_SEED;
                    break;
                }
                else if (session.loader.status != LOAD_IDLE)
                {
                    ret = CNC; // 条件不正确
                    sec_access_args.status = WAIT_CLIENT_SEED;
                    break;
                }
                else
                {
                    sec_access_args.status = SER_VERIFY_KEY;
                }
            case SER_VERIFY_KEY:
                {
                    uint16_t rcv_key = ((uint16_t)*(p_pdu->p_ser_data + 1) << 8)\
                              + *(p_pdu->p_ser_data + 2);
                    uint16_t key = 0;
                    if (sec_access_args.seed)
                    {
                        key = calc_key(sec_access_args.seed);
                        sec_access_args.seed = 0;
                    }
                    else // 万一计算出错，高概率响应NRC = IK
                    {}
                    
                    if (sec_access_args.failed_cnt\
                                >= SEC_ACCESS_FAILED_CNT_MAX)
                    {
                        sec_access_args.failed_cnt--;
                    }
                    
                    if (rcv_key == key)
                    {
                        session.sec_lev = lev_2;
                        /* TODO 构建 positive response */
                        session.tp_send_buf[0] = PR_SID;
                        session.tp_send_buf[1] = (uint8_t)sec_access_type;
                        session.ser.positive_len = 2;
                        ret = PR;
                        sec_access_args.failed_cnt = 0;
                    }
                    else
                    {
                        sec_access_args.failed_cnt++;
                        if (sec_access_args.failed_cnt\
                                >= SEC_ACCESS_FAILED_CNT_MAX)
                        {
                            // 启动禁止安全访问定时器
                            ENABLE_TIMER(sec, SEC_TIMER);
                            ret = ENOA;
                        }
                        else
                        {
                            ret = IK; // 无效的key
                        }
                    }
                    sec_access_args.status = WAIT_CLIENT_SEED;
                }
                break;
            default:
                break;
        }
    }
    else
    {
        ret = CNC; // 不需要切换安全等级
    }

    return ret;
}

static nrc_t routine_control(A_PDU_t *p_pdu) // 0x31
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else
    {
        //
    }

    return ret;
}

static nrc_t request_download(A_PDU_t *p_pdu) // 0x34
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else
    {
        /* TODO 没有明白这个inentifier的作用 */
        uint8_t data_fmt_identifier = *(p_pdu->p_ser_data + 0);
        /* TODO 没有检测start_addr 和 size 为零 */
        get_addr_and_size(p_pdu->p_ser_data + 1,\
                &session.loader.start_addr, &session.loader.size);
        
        if (lev_1 == session.sec_lev)
        {
            ret = SAD;
        }
        else if (p_pdu->len != REQUEST_DOWNLOAD_REQ_PDU_LEN)
        {
            ret = IMLOIF;
        }
        else if (NULL == session.loader.dev.check_addr_and_size\
                && NULL == session.loader.dev.write
                && NULL == session.loader.dev.read)
        {
            /* TODO 不知道理解对不对 */
            ret = CNC;
        }
        else if (session.loader.dev.check_addr_and_size(\
                    &session.loader.start_addr, &session.loader.size))
        {
            ret = ROOR;
        }
        else if (session.loader.dev.check_range(\
                    session.loader.start_addr,\
                    session.loader.start_addr + session.loader.size))
        {
            ret = IMLOIF;
        }
        else if (LOAD_IDLE == session.loader.status)
        {
            session.loader.direction = DOWNLOAD;
            session.loader.block_number = LOAD_BLOCK_NUMBER;
            session.loader.status = LOADING;
            session.tp_send_buf[0] = PR_SID;
            session.tp_send_buf[1] = LOAD_BLOCK_NUMBER;
            session.ser.positive_len = 2;
            ret = PR;
        }
        else
        {
            RESET_LOAD();
            ret = UDNA;
        }
    }

    return ret;
}

static nrc_t request_upload(A_PDU_t *p_pdu) // 0x35
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else
    {
        /* TODO 没有明白这个inentifier的作用 */
        uint8_t data_fmt_identifier = *(p_pdu->p_ser_data + 0);
        /* TODO 没有检测start_addr 和 size 为零 */
        get_addr_and_size(p_pdu->p_ser_data + 1,\
                &session.loader.start_addr, &session.loader.size);
        
        if (lev_1 == session.sec_lev)
        {
            ret = SAD;
        }
        else if (p_pdu->len != REQUEST_DOWNLOAD_REQ_PDU_LEN)
        {
            ret = IMLOIF;
        }
        else if (NULL == session.loader.dev.check_addr_and_size\
                && NULL == session.loader.dev.write
                && NULL == session.loader.dev.read)
        {
            /* TODO 不知道理解对不对 */
            ret = CNC;
        }
        else if (session.loader.dev.check_addr_and_size(\
                    &session.loader.start_addr, &session.loader.size))
        {
            ret = ROOR;
        }
        else if (session.loader.dev.check_range(\
                    session.loader.start_addr,\
                    session.loader.start_addr + session.loader.size))
        {
            ret = IMLOIF;
        }
        else if (LOAD_IDLE == session.loader.status)
        {
            session.loader.direction = UPLOAD;
            session.loader.block_number = LOAD_BLOCK_NUMBER;
            session.loader.status = LOADING;
            session.tp_send_buf[0] = PR_SID;
            session.tp_send_buf[1] = LOAD_BLOCK_NUMBER;
            session.ser.positive_len = 2;
            ret = PR;
        }
        else
        {
            RESET_LOAD();
            ret = UDNA;
        }
    }

    return ret;
}

static nrc_t transfer_data(A_PDU_t *p_pdu) // 0x36
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else if (LOADING == session.loader.status)
    {
        static uint8_t block_sequence_counter = 0;
        
        if (p_pdu->len != TRANSFER_DATA_REQ_PDU_LEN)
        {
            ret = IMLOIF;
        }
        else if (block_sequence_counter != *(p_pdu->p_ser_data))
        {
            ret = WBSC;
        }
        else if (DOWNLOAD == session.loader.direction)
        {
            // TODO 客户端请求的参数有效
            ret = ROOR;
        }
        else if (session.loader.block_number--)
        {
            if (DOWNLOAD == session.loader.direction)
            {
                uint8_t op_res = session.loader.dev.write(\
                        session.loader.start_addr,\
                        session.loader.size / LOAD_BLOCK_NUMBER);
                session.loader.start_addr += session.loader.size\
                                             / LOAD_BLOCK_NUMBER;
                if (!op_res)
                {
                    session.tp_send_buf[0] = PR_SID;
                    session.tp_send_buf[1] = session.loader.block_number;
                    session.ser.positive_len = 2;
                    ret = PR;
                }
                else
                {
                    RESET_LOAD();
                    // TODO 需要重新请求
                    ret = GPF;
                }
            }
            else
            {
                uint8_t op_res = session.loader.dev.read(\
                        session.loader.start_addr,\
                        session.loader.size / LOAD_BLOCK_NUMBER);
                session.loader.start_addr += session.loader.size\
                                             / LOAD_BLOCK_NUMBER;
                if (!op_res)
                {
                    session.tp_send_buf[0] = PR_SID;
                    session.tp_send_buf[1] = session.loader.block_number;
                    session.ser.positive_len = 2;
                    ret = PR;
                }
                else
                {
                    RESET_LOAD();
                    // TODO 需要重新请求
                    ret = GPF;
                }
            }
        }
        
        if (!session.loader.block_number) // load completed
        {
            session.loader.status = LOAD_COMPLETED;
        }
    }
    else
    {
        RESET_LOAD();
        ret = RSE;
    }

    return ret;
}

static nrc_t request_transfer_exit(A_PDU_t *p_pdu) // 0x37
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else if (LOAD_COMPLETED == session.loader.status)
    {
        if (p_pdu->len != REQUEST_TRANSFER_EXIT_REQ_PDU_LEN)
        {
            ret = IMLOIF;
        }
        else if (0)
        {
            // TODO 客户端请求的参数有效
            ret = ROOR;
        }
        else
        {
            RESET_LOAD();
            // TODO 需要具体细节
            session.tp_send_buf[0] = PR_SID;
            session.tp_send_buf[1] = 4;
            session.ser.positive_len = 2;
            ret = PR; // mark positive response
        }
    }
    else
    {
        RESET_LOAD();
        ret = RSE;
    }

    return ret;
}

static nrc_t ecu_reset(A_PDU_t *p_pdu) // 0x11
{
    nrc_t ret = UNRC;

    if (NULL == p_pdu && NULL == p_pdu->p_ser_data && 0 == p_pdu->len)
    {
        ret = UNRC;
    }
    else
    {
        reset_type_t type = HARD_RESET;
        
        if (type > SOFT_RESET)
        {
            ret = SFNS; // 不支持的子功能
        }
        else if (p_pdu->len != ECU_RESET_REQ_PDU_LEN)
        {
            ret = IMLOIF; // 不正确的信息长度或无效格式
        }
        else if (NULL == session.mcu.reset\
                && session.loader.status != LOAD_IDLE)
        {
            ret = CNC; // 条件不正确
        }
        else
        {
            /* TODO 构建positive response  */
            session.tp_send_buf[0] = PR_SID;
            session.tp_send_buf[1] = (uint8_t)type;
            session.ser.positive_len = 2;
            ret = PR; // mark positive response
            response(ret);
            session.mcu.reset(type);
        }
    }

    return ret;
}

/* 服务函数定义 end */


