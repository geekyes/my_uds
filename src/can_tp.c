#include "CAN_TP.h"

#undef NULL
#define NULL ((void*)0)
#define TRUE (1U)
#define FALSE (0U)

#define SINGLE_FRAME ((uint8_t)0x00)
#define FIRST_FRAME  ((uint8_t)0x10)
#define CONSECUTIVE_FRAME ((uint8_t)0x20)
#define FLOW_CONTROL_FRAME ((uint8_t)0x30)
#define UNKOWN_FRAME ((uint8_t)0xff)

#define CAN_FRAME_TYPE_MASK ((uint8_t)0x30)
#define LOW_HALF_OF_BYTE_MASK ((uint8_t)0x0f)

#define FC_BS_BAX ((uint8_t)0xff)
#define FC_STMIN_BAX ((uint8_t)0x80)

#define TIMER(USER, TIMER_NAME) do {\
    if (N_runtime_data.USER.timer.TIMER_NAME.is_valid) {\
        if (N_runtime_data.USER.timer.TIMER_NAME.cnt) {\
            N_runtime_data.USER.timer.TIMER_NAME.status = TIMER_TIMING;\
            N_runtime_data.USER.timer.TIMER_NAME.cnt--;\
        }\
        else {\
            N_runtime_data.USER.timer.TIMER_NAME.cnt = 0;\
            N_runtime_data.USER.timer.TIMER_NAME.is_valid = TIMER_STOP;\
            N_runtime_data.USER.timer.TIMER_NAME.status = TIMER_TIMEOUT;\
        }\
    }\
} while (0)
#define ENABLE_TIMER(USER, TIMER_NAME, CNT) do {\
    N_runtime_data.USER.timer.TIMER_NAME.cnt = CNT;\
    N_runtime_data.USER.timer.TIMER_NAME.status = TIMER_TIMING;\
    N_runtime_data.USER.timer.TIMER_NAME.is_valid = TIMER_START;\
} while (0)
#define DISENABLE_TIMER(USER, TIMER_NAME) do {\
    N_runtime_data.USER.timer.TIMER_NAME.cnt = 0;\
    N_runtime_data.USER.timer.TIMER_NAME.status = TIMER_TIMEOUT;\
    N_runtime_data.USER.timer.TIMER_NAME.is_valid = TIMER_STOP;\
} while (0)
#define GET_TIMER_STATUS(USER, TIMER_NAME)\
    (N_runtime_data.USER.timer.TIMER_NAME.status)

#define RCV_SEND_FC(FS_VALUE) do {\
        N_runtime_data.rcv.FC.FS = FS_VALUE;\
        N_runtime_data.rcv.FC.BS = FC_BS;\
        N_runtime_data.rcv.FC.STmin = TIMER_FC_STMIN;\
        N_runtime_data.rcv.FC.is_valid = DATA_VALID;\
        N_runtime_data.rcv.status = SEND_FC;\
        ENABLE_TIMER(rcv, N_Br, TIMER_B_R);\
} while (0)

#define SWITCH_TO_RCV_OK(N_RESULT) do {\
    N_runtime_data.rcv.N_result = N_RESULT;\
    N_runtime_data.rcv.status = RCV_OK;\
} while (0)

typedef enum {TIMER_STOP, TIMER_START}timer_validity_t; // 有效就定时
typedef enum {TIMER_TIMING, TIMER_TIMEOUT}timer_status_t; // 定时器的状态

typedef struct {
    N_AI_t N_AI;
    uint32_t length; // 数据实际长度
    uint8_t buf[N_DATA_MAX];
}N_buffer_t; // 网络层的数据缓冲区

typedef enum {
    TX_IDLE, SEND_FF, WAIT_FC, SEND_CF, SEND_OK
}Tx_flow_t; // 发送状态机的状态们

typedef enum {
    RX_IDLE, RCV_FF, SEND_FC, RCV_CF, RCV_OK
}Rx_flow_t; // 接收状态机的状态们

typedef enum {CTS, WT, OVFLW, RESERVED}FC_FS_t;


typedef struct {
    timer_validity_t is_valid; // TIMER_STOP: 停止定时器 TIMER_START: 开始定时
    timer_status_t status; // TIMER_TIMING: 定时中 TIMER_TIMEOUT: 定时时间到 
    uint16_t cnt;
}timer_t;

typedef struct {
    validity_t is_valid; // VALID: 需要发送流控帧 INVALID 
    FC_FS_t FS; // 流状态参数
    uint8_t STmin; // 间隔时间
    uint8_t BS; // 块大小
}FC_param_t;

typedef struct {
    timer_t N_As;
    timer_t N_Bs;
    timer_t N_Cs;
    timer_t STmin;
}Tx_timer_t;

typedef struct {
    N_result_t N_result;
    N_PDU_t N_PDU;
    uint8_t *p_data; // TODO 没有验证是否为NULL
    uint32_t length;
    Tx_flow_t status;
    uint8_t CF_SN; // 流控帧编号
    FC_param_t FC;
    Tx_timer_t timer;
}Tx_runtime_data_t; // 数据缓冲区的指针，用于拆分缓冲区数据

typedef struct timer {
    timer_t N_Ar;
    timer_t N_Br;
    timer_t N_Cr;
}Rx_timer_t;

typedef struct {
    N_result_t N_result;
    N_PDU_t N_PDU;
    uint8_t *p_data; // TODO 没有验证是否为NULL
    uint32_t length;
    Rx_flow_t status;
    uint8_t CF_SN; // 流控帧编号
    FC_param_t FC;
    Rx_timer_t timer;
}Rx_runtime_data_t; // 数据缓冲区的指针，用于拆分缓冲区数据

typedef struct {
    N_buffer_t Tx_buf; // 发送数据的缓冲区
    N_buffer_t Rx_buf; // 接收数据的缓冲区
    Tx_runtime_data_t send;
    Rx_runtime_data_t rcv;
}N_runtime_data_t;

static N_runtime_data_t N_runtime_data;
static fp_link_layer_t fp_send_ll = NULL;
static fp_link_layer_t fp_rcv_ll = NULL;

///////////////////////下面的函数需要根据具体的单片机编写///////////////////////
// 将N_PDU转换到L_PDU，并调用数据链路层的API发送
static void send_link_layer(N_PDU_t *p_pdu);
// 将L_PDU转换到N_PDU，并调用数据链路层的API接收
static void receive_link_layer(N_PDU_t *p_pdu);
////////////////////////////////////////////////////////////////////////////////

// 发送数据长度小于7U的数据
static void send_single_frame(N_PDU_t *p_pdu);
// 发送数据长度大于7U的数据
static void send_multiple_frame(N_PDU_t *p_pdu);
// 发送一个流控帧
static void send_flow_ctrl_frame(N_PDU_t *p_pdu);

// (single frame)将N_PDU的数据保存到N_runtime_data.Rx_buf
static void rcv_single_frame(N_PDU_t *p_pdu);
// (multiple frame)将N_PDU的数据保存到N_runtime_data.Rx_buf
static void rcv_first_frame(N_PDU_t *p_pdu);
static void rcv_consecutive_frame(N_PDU_t *p_pdu);
static void rcv_multiple_frame(N_PDU_t *p_pdu);
// 将接收到的流控帧参数赋值给N_runtime_data.send.FC
static void rcv_flow_ctrl_frame(N_PDU_t *p_pdu);

// 定时器定时控制
static void timer_ctrl_network_layer(void);

// 将p_src的数数据拷贝到p_dest
static void data_copy(uint8_t *p_dest, const uint8_t *p_src, uint8_t size);
// 将p_data以字节填充为fill
static void data_fill(uint8_t *p_data, uint8_t fill, uint8_t size);

// network_layer初始化
void init_network_layer(fp_link_layer_t send, fp_link_layer_t rcv)
{
    fp_send_ll = send;
    fp_rcv_ll = rcv;
    /* TODO 定时器出错，因为没有判断是否定时器是否有效，所以需要初始化 */
    DISENABLE_TIMER(send, N_As);
    DISENABLE_TIMER(send, N_Bs);
    //DISENABLE_TIMER(send, N_Cs); // 不使用，自己认为跟STmin定时器重合了
    DISENABLE_TIMER(send, STmin); // 发送CF与CF之间的间隔定时器
    // 接收定时器们
    //DISENABLE_TIMER(rcv, N_Ar); // 不使用，没有独立发送流控帧，所以相当于N_As
    DISENABLE_TIMER(rcv, N_Br);
    DISENABLE_TIMER(rcv, N_Cr);
}

// 需要周期调用，主要是对时间参数和数据流的控制
void main_network_layer(void)
{
    {
        // 有数据需要发送 或者 N_PDU还没有发送出去
        if (N_runtime_data.Tx_buf.length\
                || DATA_VALID == N_runtime_data.send.N_PDU.is_valid) { 
            if (DATA_INVALID == N_runtime_data.send.N_PDU.is_valid) {
                // 获取地址信息
                N_runtime_data.send.N_PDU.N_AI = N_runtime_data.Tx_buf.N_AI;
                if (N_runtime_data.Tx_buf.length < CAN_FRAME_SIZE) { 
                    send_single_frame(&N_runtime_data.send.N_PDU); // 构造一个SF
                }
                else { // 拆分数据
                    // 调用一次，构造一个Tx_N_PDU
                    send_multiple_frame(&N_runtime_data.send.N_PDU);
                }
            }
            // 自己认为这个判断有点多余，但是...纠结
            if (DATA_VALID == N_runtime_data.send.N_PDU.is_valid) {
                // 将N_PDU转换到L_PDU，并调用数据链路层的API发送
                send_link_layer(&N_runtime_data.send.N_PDU);
            }
        }
        
        // 发送流控帧，只有在接收的时候才会发送，所以归类为rcv的运行时参数
        if (DATA_VALID == N_runtime_data.rcv.FC.is_valid\
                && DATA_INVALID == N_runtime_data.send.N_PDU.is_valid) {
            send_flow_ctrl_frame(&N_runtime_data.send.N_PDU); // 构造一个FC
            // 说明数据成功装载到 N_PDU，标记send.FC已经使用
            N_runtime_data.rcv.FC.is_valid = DATA_INVALID;
            if (DATA_VALID == N_runtime_data.send.N_PDU.is_valid) {
                // 将N_PDU转换到L_PDU，并调用数据链路层的API发送
                send_link_layer(&N_runtime_data.send.N_PDU);
            }
        }
    }
    
    {
        static uint8_t frame_type = UNKOWN_FRAME;
        receive_link_layer(&N_runtime_data.rcv.N_PDU);
        if (DATA_VALID == N_runtime_data.rcv.N_PDU.is_valid) {
            frame_type = N_runtime_data.rcv.N_PDU.N_PCI_Data[0]\
                         & CAN_FRAME_TYPE_MASK;
            // 当在接收到FF后，如果接到除开CF以外都 TODO 忽略
            // TODO 流程设计不合理，其状态不应该在rcv_multiple_frame()函数外使用
            if (SINGLE_FRAME == frame_type\
                    && N_runtime_data.rcv.status != RX_IDLE) {
                frame_type = UNKOWN_FRAME;
            }
        }
        
        switch (frame_type) {
            case SINGLE_FRAME:
                // 清除上次接收的结果
                N_runtime_data.rcv.N_result = N_INVALID;
                rcv_single_frame(&N_runtime_data.rcv.N_PDU);
                frame_type = UNKOWN_FRAME;
                break;
            case FIRST_FRAME:
            case CONSECUTIVE_FRAME:
                rcv_multiple_frame(&N_runtime_data.rcv.N_PDU);
                /* 保证把multiple_frame的流程走完 */
                if (RX_IDLE == N_runtime_data.rcv.status) {
                    frame_type = UNKOWN_FRAME;
                }
                break;
            case FLOW_CONTROL_FRAME:
                rcv_flow_ctrl_frame(&N_runtime_data.rcv.N_PDU);
                frame_type = UNKOWN_FRAME;
                break;
            case UNKOWN_FRAME: // TODO 如果接收到这样的帧头，就jj了
                break;
            default:
                /* 应该停止接收multiple frame */
                SWITCH_TO_RCV_OK(N_UNEXP_PDU);
                break;
        }
    }

    // 时间控制，依赖本函数的调用周期
    timer_ctrl_network_layer();
}

// 将要发送的数据缓存到数据缓存区
void send_network_layer(N_AI_t addr, const uint8_t *p_data, uint32_t size)
{
    if(NULL == p_data && (0 == size || size >= N_DATA_MAX)) {
        // 记录出错
        return;
    }

    // TODO 处理若当前没有发送完，又有数据需要发送，是不允许还是加入缓冲区

    if (TX_IDLE == N_runtime_data.send.status) {
        // 地址的处理
        N_runtime_data.Tx_buf.N_AI = addr;
        // 拷贝数据
        for (uint32_t i = 0; i < size; i++) {
            N_runtime_data.Tx_buf.buf[i] = p_data[i];
        }
        // 保存数据长度
        N_runtime_data.Tx_buf.length = size;
        /* 清除上次的发送结果 */
        N_runtime_data.send.N_result = N_INVALID;
    }
}

// 提供发送数据的状态
N_result_t confirm_network_layer(void)
{
    return N_runtime_data.send.N_result;
}

// 提供接收到的所有数据
// 返回{.N_result == N_OK}，
// 返回{.N_result == N_INVALID}表示在接收中或是没有接收到数据
rcv_result_t receive_network_layer(N_AI_t addr, uint8_t *p_data, uint32_t size_max)
{
    rcv_result_t result = {.N_result = N_ERROR};
    
    if (NULL == p_data\
            && (0 == size_max || size_max < N_runtime_data.Rx_buf.length)) {
        return result;
    }

    // 获取接收的结果(优先级：地址不匹配错误 > 网络层的N_result)
    result.N_result = N_runtime_data.rcv.N_result;


    // 当N_Result值为N_OK时，<MessageDate>及<Length>参数信息才有效
    if (N_OK == result.N_result) {
        // 地址验证不通过
        if (addr.id == N_runtime_data.Rx_buf.N_AI.id\
                && addr.N_TAtype == N_runtime_data.Rx_buf.N_AI.N_TAtype) {
        
        // 拷贝数据
        for (uint32_t i = 0; i < N_runtime_data.Rx_buf.length; i++) {
            p_data[i] = N_runtime_data.Rx_buf.buf[i];
        }

        // 接收数据的长度
        result.length = N_runtime_data.Rx_buf.length;
        // (假)清除Rx_buf的数据
        N_runtime_data.Rx_buf.length = 0;
        // 清除接收的结果
        N_runtime_data.rcv.N_result = N_INVALID;
        //result.N_result = N_OK/*N_runtime_data.rcv.N_result*/;
        }
        else {
            result.N_result = N_ERROR;
        }
    }
    


    return result;
}

// 通知上层验证N_AI，自己测试没用使用这个函数
validity_t receive_FF_network_layer(N_AI_t *p_addr)
{
    validity_t is_valid = DATA_INVALID;
    
    if (NULL == p_addr) {
        return DATA_INVALID;
    }

    // FF已经接收完毕 TODO 没有明确本函数的支持
    if (SEND_FC == N_runtime_data.rcv.status) {
        // 地址验证不通过
        if (p_addr->id != N_runtime_data.rcv.N_PDU.N_AI.id\
                || p_addr->N_TAtype != N_runtime_data.rcv.N_PDU.N_AI.N_TAtype) {
            // 不接收
            SWITCH_TO_RCV_OK(N_ERROR);
            is_valid = DATA_INVALID;
        }
        else {
            // 验证通过发送流控帧
            RCV_SEND_FC(CTS);
            is_valid = DATA_VALID;
        }
    }

    return is_valid;
}

// 发送数据长度小于7U的数据
static void send_single_frame(N_PDU_t *p_pdu)
{
    if (NULL == p_pdu) {
        return;
    }

    // 设置单帧的PCI
    p_pdu->N_PCI_Data[0] = (uint8_t)N_runtime_data.Tx_buf.length\
                           | SINGLE_FRAME;
    data_copy(p_pdu->N_PCI_Data + 1,\
            N_runtime_data.Tx_buf.buf,\
            N_runtime_data.Tx_buf.length);
    // 填充剩余字节
    data_fill(p_pdu->N_PCI_Data + N_runtime_data.Tx_buf.length + 1,\
            CAN_FRAME_BYTE_FILL,\
            CAN_FRAME_SIZE - 1 - N_runtime_data.Tx_buf.length);
    p_pdu->is_valid = DATA_VALID; // 标记数据有效
    // 清除发送缓冲区的数据 (不是真正的清除)
    N_runtime_data.Tx_buf.length = 0;
    N_runtime_data.send.N_result = N_OK;
}

// 发送数据长度大于7U的数据
static void send_multiple_frame(N_PDU_t *p_pdu)
{
    uint8_t start_data_byte = 0;
    uint8_t send_data_size = 0; // 是指的一个N_PDU减去N_PCI和N_AI的字节数

    if (NULL == p_pdu) {
        return;
    }

    switch (N_runtime_data.send.status) {
        case SEND_FF:
            // 获取要发送数据的头指针和数据大小
            N_runtime_data.send.p_data = N_runtime_data.Tx_buf.buf;
            N_runtime_data.send.length = N_runtime_data.Tx_buf.length;
            // 构造FF N_PDU
            p_pdu->N_PCI_Data[0] = (uint8_t)(N_runtime_data.send.length >> 8U)\
                                   | FIRST_FRAME;
            p_pdu->N_PCI_Data[1] = (uint8_t)(N_runtime_data.send.length);
            // 因为上层函数已经验证了数据长度大于8，所以不会出现 数组访问越界
            start_data_byte = 2;
            send_data_size = CAN_FRAME_SIZE - start_data_byte;
            data_copy(p_pdu->N_PCI_Data + start_data_byte,\
                    N_runtime_data.send.p_data, send_data_size);
            p_pdu->is_valid = DATA_VALID; // 标记数据有效
            // (数据拆分)从缓冲区移除即将发生的数据
            N_runtime_data.send.p_data += send_data_size;
            N_runtime_data.send.length -= send_data_size;
            // 切换到下一个状态
            N_runtime_data.send.status = WAIT_FC;
            N_runtime_data.send.CF_SN = 1; //设置流控帧编号为1
            break;
        case WAIT_FC:
            {
                // 定时器只启动一次
                static uint8_t flag = 1;
                if (flag) {
                    flag = 0;
                    ENABLE_TIMER(send, N_Bs, TIMER_B_S);
                }
                
                if (DATA_VALID == N_runtime_data.send.FC.is_valid\
                        && TIMER_TIMING == GET_TIMER_STATUS(send, N_Bs)) {
                    flag = 1;
                    // 发送第一帧，不用等待
                    ENABLE_TIMER(send, STmin, 0);
                    N_runtime_data.send.status = SEND_CF;
                }
                else if (TIMER_TIMEOUT == GET_TIMER_STATUS(send, N_Bs)){
                    flag = 1;
                    N_runtime_data.send.status = SEND_OK;
                    N_runtime_data.send.N_result = N_TIMEOUT_Bs;
                }
            }
            break;
        case SEND_CF:
            switch (N_runtime_data.send.FC.FS) {
                case CTS: // 继续发送
                    if (TIMER_TIMEOUT == GET_TIMER_STATUS(send, STmin)) {
                        // 构造CF N_PDU
                        p_pdu->N_PCI_Data[0] = N_runtime_data.send.CF_SN\
                                               | CONSECUTIVE_FRAME;
                        N_runtime_data.send.CF_SN++;
                        if (N_runtime_data.send.CF_SN > 0xf) { // SN只有4bit
                            N_runtime_data.send.CF_SN = 0;
                        }
                        /* 处理参数BS */
                        if (N_runtime_data.send.FC.BS == 1) {
                            N_runtime_data.send.FC.is_valid = DATA_INVALID;
                            N_runtime_data.send.status = WAIT_FC;
                            /*
                             * // TODO 设置流控帧编号为0
                             * N_runtime_data.send.CF_SN = 0;
                             */
                        }
                        N_runtime_data.send.FC.BS--;
                        
                        start_data_byte = 1;
                        send_data_size = CAN_FRAME_SIZE - start_data_byte;
                        if (send_data_size < N_runtime_data.send.length) {
                            data_copy(p_pdu->N_PCI_Data + start_data_byte,\
                                    N_runtime_data.send.p_data,\
                                    send_data_size);
                            p_pdu->is_valid = DATA_VALID; // 标记数据有效
                            // (数据拆分)从缓冲区移除即将发生的数据
                            N_runtime_data.send.p_data += send_data_size;
                            N_runtime_data.send.length -= send_data_size;
                        }
                        else {
                            data_copy(p_pdu->N_PCI_Data + start_data_byte,\
                                      N_runtime_data.send.p_data,\
                                      (uint8_t)N_runtime_data.send.length);
                            data_fill(p_pdu->N_PCI_Data +\
                                    N_runtime_data.send.length + 1,\
                                    CAN_FRAME_BYTE_FILL,\
                                    CAN_FRAME_SIZE - start_data_byte - \
                                    (uint8_t)N_runtime_data.send.length);
                            p_pdu->is_valid = DATA_VALID; // 标记数据有效
                            // 切换到下一个状态
                            N_runtime_data.send.status = SEND_OK;
                            N_runtime_data.send.N_result = N_OK;
                        }
                        // STmin只支持ms，其CNT = STmin / MAIN_NETWORK_LAYER_PERIOD
                        ENABLE_TIMER(send, STmin, (N_runtime_data.send.FC.STmin\
                                / MAIN_NETWORK_LAYER_PERIOD));
                    }
                    break;
                case WT: // 等待，即重置N_Bs定时器
                    ENABLE_TIMER(send, N_Bs, TIMER_B_S);
                    // TODO 等待的个数 N_WFT_OVRN
                    break;
                case OVFLW: // 对方不能接收这个长的数据 
                    N_runtime_data.send.N_result = N_BUFFER_OVFLW;
                    N_runtime_data.send.status = SEND_OK;
                    break;
                case RESERVED:
                    break;
                default:
                    break;
            }
            break;
        case SEND_OK:
            // 清除发送缓冲区的数据 (不是真正的清除)
            N_runtime_data.Tx_buf.length = 0;
            // 清除缓冲区指针和数据长度
            N_runtime_data.send.N_PDU.is_valid = DATA_INVALID;
            N_runtime_data.send.p_data = NULL;
            N_runtime_data.send.length = 0;
            // 切换到TX_IDLE
            N_runtime_data.send.status = TX_IDLE;
            // 清除网络层发送的参数
            N_runtime_data.send.CF_SN = 0;
            N_runtime_data.send.FC.is_valid = DATA_INVALID;
            // 关闭所有send的定时器
            //DISENABLE_TIMER(send, N_As);
            DISENABLE_TIMER(send, N_Bs);
            //DISENABLE_TIMER(send, N_Cs);
            DISENABLE_TIMER(send, STmin);
            break;
        case TX_IDLE:
        default:
            if (N_runtime_data.Tx_buf.length >= CAN_FRAME_SIZE) {
                N_runtime_data.send.status = SEND_FF;
            }
            break;
    }
}

// 发送一个流控帧
static void send_flow_ctrl_frame(N_PDU_t *p_pdu)
{
    if (NULL == p_pdu) {
        return;
    }

    p_pdu->N_PCI_Data[0] = N_runtime_data.rcv.FC.FS | FLOW_CONTROL_FRAME;
    p_pdu->N_PCI_Data[1] = N_runtime_data.rcv.FC.BS;
    p_pdu->N_PCI_Data[2] = N_runtime_data.rcv.FC.STmin;
    data_fill(p_pdu->N_PCI_Data + 3, CAN_FRAME_BYTE_FILL, CAN_FRAME_SIZE - 3);
    p_pdu->is_valid = DATA_VALID;
}

// (single frame)将N_PDU的数据保存到N_runtime_data.Rx_buf
static void rcv_single_frame(N_PDU_t *p_pdu)
{
    uint8_t SF_DL = 0;
    uint8_t start_data_byte = 1;
    uint8_t data_size = CAN_FRAME_SIZE - start_data_byte;
    
    if (NULL == p_pdu) {
        return;
    }

    // 获取地址信息
    N_runtime_data.Rx_buf.N_AI = p_pdu->N_AI;
    // 获取并验证SF_DL
    SF_DL = p_pdu->N_PCI_Data[0] & LOW_HALF_OF_BYTE_MASK;
    if (0 != SF_DL && SF_DL <= data_size) {
        data_copy(N_runtime_data.Rx_buf.buf,\
                p_pdu->N_PCI_Data + start_data_byte,\
                data_size);
        N_runtime_data.Rx_buf.length = SF_DL;
        p_pdu->is_valid = DATA_INVALID;
        N_runtime_data.rcv.N_result = N_OK;
    }
    else {
        N_runtime_data.rcv.N_result = N_ERROR;
    }
}

// (multiple frame)将N_PDU的数据保存到N_runtime_data.Rx_buf
static void rcv_first_frame(N_PDU_t *p_pdu)
{
    uint16_t FF_DL = 0;
    uint8_t start_data_byte = 2;
    uint8_t data_size = CAN_FRAME_SIZE - start_data_byte;
    
    if (NULL == p_pdu) {
        return;
    }

    // 获取地址信息
    N_runtime_data.Rx_buf.N_AI = p_pdu->N_AI;
    // 获取并验证FF_DL
    FF_DL = (uint16_t)(p_pdu->N_PCI_Data[0] & LOW_HALF_OF_BYTE_MASK) << 8U;
    FF_DL |= p_pdu->N_PCI_Data[1];
    if (FF_DL > data_size && FF_DL <= N_DATA_MAX) {
        // 设置要接收数据的头指针和数据大小
        N_runtime_data.rcv.p_data = N_runtime_data.Rx_buf.buf;
        N_runtime_data.Rx_buf.length = FF_DL;
        N_runtime_data.rcv.length = FF_DL;
        data_copy(N_runtime_data.rcv.p_data,\
                p_pdu->N_PCI_Data + start_data_byte,\
                data_size);
        // (数据重组)移动数据的头指针和数据大小
        N_runtime_data.rcv.p_data += data_size;
        N_runtime_data.rcv.length -= data_size;
        // 设置CF_SN
        N_runtime_data.rcv.CF_SN = 1;
        /* TODO 不使用receive_FF_network_layer确认 */
        RCV_SEND_FC(CTS);
        // TODO 首帧是第一个BS
        N_runtime_data.rcv.FC.BS -= 1;
    }
    else if (FF_DL > N_DATA_MAX) { // FF_DL出错
        RCV_SEND_FC(OVFLW);
    }
    p_pdu->is_valid = DATA_INVALID;
    // 清除上次接收的结果
    N_runtime_data.rcv.N_result = N_INVALID;
}

// (multiple frame)将N_PDU的数据保存到N_runtime_data.Rx_buf
static void rcv_consecutive_frame(N_PDU_t *p_pdu)
{
    uint8_t SN = 0;
    uint8_t start_data_byte = 1;
    uint8_t data_size = CAN_FRAME_SIZE - start_data_byte;

    if (NULL == p_pdu) {
        return;
    }

    // 获取并验证SN
    SN = p_pdu->N_PCI_Data[0] & LOW_HALF_OF_BYTE_MASK;
    if (SN == N_runtime_data.rcv.CF_SN) {
        /* CF_SN计算 */
        N_runtime_data.rcv.CF_SN++;
        if (N_runtime_data.rcv.CF_SN > 0xf) { // SN只有4bit
            N_runtime_data.rcv.CF_SN = 0;
        }
        // 接收需要处理BS
        if (N_runtime_data.rcv.FC.BS == 1) {
            /*
             * // TODO 设置CF_SN
             * N_runtime_data.rcv.CF_SN = 0;
             */
            RCV_SEND_FC(CTS);
        }
        else {
            N_runtime_data.rcv.FC.BS--;
        }
        
        if (data_size < N_runtime_data.rcv.length) {
            data_copy(N_runtime_data.rcv.p_data,\
                    p_pdu->N_PCI_Data + start_data_byte,\
                    data_size);
            N_runtime_data.rcv.p_data += data_size;
            N_runtime_data.rcv.length -= data_size;
        }
        else {
            data_copy(N_runtime_data.rcv.p_data,\
                    p_pdu->N_PCI_Data + start_data_byte,\
                    (uint8_t)N_runtime_data.rcv.length);
            // 接收结束，运行时参数处理
            SWITCH_TO_RCV_OK(N_OK);
        }
    }
    else {
        SWITCH_TO_RCV_OK(N_WRONG_SN);
    }
    p_pdu->is_valid = DATA_INVALID;
}

// (multiple frame)将N_PDU的数据保存到N_runtime_data.Rx_buf
static void rcv_multiple_frame(N_PDU_t *p_pdu)
{
    if (NULL == p_pdu) {
        return;
    }

    switch (N_runtime_data.rcv.status) {
        // 通过receive_FF_network_layer函数验证地址，并切换到SEND_FC状态
        case RCV_FF:
            rcv_first_frame(p_pdu);
            break;
        case SEND_FC:
            if (N_TIMEOUT_A == N_runtime_data.send.N_result) { // N_Ar超时
                N_runtime_data.rcv.N_result = N_TIMEOUT_A;
                N_runtime_data.rcv.status = RCV_OK;
            }
            // FC已经装载到N_PDU
            else if (DATA_INVALID == N_runtime_data.rcv.FC.is_valid\
                    && TIMER_TIMING == GET_TIMER_STATUS(rcv, N_Br)) {
                DISENABLE_TIMER(rcv, N_Br);
                switch (N_runtime_data.rcv.FC.FS) {
                    case CTS:
                        N_runtime_data.rcv.status = RCV_CF;
                        break;
                    case WT:
                        break;
                    case OVFLW:
                        SWITCH_TO_RCV_OK(N_ERROR);
                        break;
                    case RESERVED:
                        break;
                    default:
                        break;
                }
                ENABLE_TIMER(rcv, N_Cr, TIMER_C_R);
            }
            else if (TIMER_TIMEOUT == GET_TIMER_STATUS(rcv, N_Br)) {
                SWITCH_TO_RCV_OK(N_ERROR);
            }
            break;
        case RCV_CF:
            // 接收到CF，且没有超时N_Cr
            if (DATA_VALID == p_pdu->is_valid\
                    && TIMER_TIMING == GET_TIMER_STATUS(rcv, N_Cr)) {
                ENABLE_TIMER(rcv, N_Cr, TIMER_C_R);
                rcv_consecutive_frame(p_pdu);
            }
            else if (TIMER_TIMEOUT == GET_TIMER_STATUS(rcv, N_Cr)) { // N_Cr超时
                SWITCH_TO_RCV_OK(N_TIMEOUT_Cr);
            }
            break;
        case RCV_OK:
            // 清除缓冲区指针和数据长度
            N_runtime_data.rcv.N_PDU.is_valid = DATA_INVALID;
            N_runtime_data.rcv.p_data = NULL;
            N_runtime_data.rcv.length = 0;
            // 切换到RX_IDLE
            N_runtime_data.rcv.status = RX_IDLE;
            // 清除网络层发送的参数
            N_runtime_data.rcv.CF_SN = 0;
            N_runtime_data.rcv.FC.is_valid = DATA_INVALID;
            // 关闭所有rcv的定时器
            //DISENABLE_TIMER(rcv, N_Ar);
            DISENABLE_TIMER(rcv, N_Br);
            DISENABLE_TIMER(rcv, N_Cr);
            break;
        case RX_IDLE:
            if (FIRST_FRAME == (p_pdu->N_PCI_Data[0] & CAN_FRAME_TYPE_MASK)) {
                N_runtime_data.rcv.status = RCV_FF;
            }
            break;
        default:
            break;
    }
}

// 将接收到的流控帧参数赋值给N_runtime_data.send.FC
static void rcv_flow_ctrl_frame(N_PDU_t *p_pdu)
{
    if (NULL == p_pdu) {
        return;
    }
    
    N_runtime_data.send.FC.FS = p_pdu->N_PCI_Data[0] & LOW_HALF_OF_BYTE_MASK;
    N_runtime_data.send.FC.BS = p_pdu->N_PCI_Data[1];
    N_runtime_data.send.FC.STmin = p_pdu->N_PCI_Data[2];
    // TODO 验证FC的参数有意义，STmin只支持ms
    if (N_runtime_data.send.FC.FS < RESERVED\
            && N_runtime_data.send.FC.BS < FC_BS_BAX\
            && N_runtime_data.send.FC.STmin < FC_STMIN_BAX) {
        N_runtime_data.send.FC.is_valid = DATA_VALID;
    }
    else {
        // 这由谁报告: receive_network_layer
        N_runtime_data.send.N_result = N_INVALID_FS;
    }
    p_pdu->is_valid = DATA_INVALID;
 
}

// 定时器定时控制
static void timer_ctrl_network_layer(void)
{
    // 发送定时器们
    TIMER(send, N_As);
    TIMER(send, N_Bs);
    //TIMER(send, N_Cs); // 不使用，自己认为跟STmin定时器重合了
    TIMER(send, STmin); // 发送CF与CF之间的间隔定时器
    // 接收定时器们
    //TIMER(rcv, N_Ar); // 不使用，没有独立发送流控帧，所以相当于N_As
    TIMER(rcv, N_Br);
    TIMER(rcv, N_Cr);
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

// 将N_PDU转换到L_PDU，并调用数据链路层的API发送
static void send_link_layer(N_PDU_t *p_pdu)
{
    // 保证发送函数只执行一次的flag
    static uint8_t flag = 1;
    
    if (NULL == p_pdu) {
        // 错误记录
        return;
    }

    if (flag) { // 执行一次
        flag = 0;
        fp_send_ll(p_pdu);
        // 使能定时器N_As
        ENABLE_TIMER(send, N_As, TIMER_A_S);
    }

    if (TRUE) {
        flag = 1;
        // 失能定时器N_As
        DISENABLE_TIMER(send, N_As);
        N_runtime_data.send.N_PDU.is_valid = DATA_INVALID;
    }
    else if (TIMER_TIMEOUT == GET_TIMER_STATUS(send, N_As)) { // 超时处理
        flag = 1;
        // 失能定时器N_As
        DISENABLE_TIMER(send, N_As);
        N_runtime_data.send.N_result = N_TIMEOUT_A;
    }
}

// 将L_PDU转换到N_PDU，并调用数据链路层的API接收
static void receive_link_layer(N_PDU_t *p_pdu)
{
    if (NULL == p_pdu) {
        // 错误记录
        return;
    }

    fp_rcv_ll(p_pdu);
}


