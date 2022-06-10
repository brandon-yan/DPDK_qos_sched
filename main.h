//
// Created by hanchong on 2022/6/10.
//

#ifndef DKDP_QOS_MAIN_H
#define DKDP_QOS_MAIN_H

#define APP_RX_DESC_DEFAULT 1024
#define APP_TX_DESC_DEFAULT 1024

#define APP_RING_SIZE (8*1024)
#define NB_MBUF   (2*1024*1024)

#define MAX_PKT_RX_BURST 64
#define MAX_PKT_TX_BURST 64
#define PKT_ENQUEUE 64
#define PKT_DEQUEUE 32

#define APP_INTERACTIVE_DEFAULT 0
#define APP_QAVG_PERIOD 100
#define APP_QAVG_NTIMES 10
#define APP_MAX_LCORE 64
#define MAX_DATA_STREAMS 32

#ifndef APP_COLLECT_STAT
#define APP_COLLECT_STAT		1
#endif

#if APP_COLLECT_STAT
#define APP_STATS_ADD(stat,val) (stat) += (val)
#else
#define APP_STATS_ADD(stat,val) do {(void) (val);} while (0)
#endif

#define BURST_TX_DRAIN_US 100

struct thread_stat {
    uint64_t nb_rx, nb_drop;
};

struct ring_conf {
    uint32_t rx_size, tx_size, ring_size;
};

struct burst_conf {
    uint16_t rx_burst, tx_burst, ring_burst, qos_dequeue;
};
struct thread_conf {
    uint32_t counter, n_mbufs;
    struct rte_mbuf **m_table;

    uint16_t rx_port, tx_port, rx_queue, tx_queue;
    struct rte_ring *rx_ring;
    struct rte_ring *tx_ring;
    struct rte_sched_port *sched_port;

    struct thread_stat stat;
};

struct flow_conf {
    uint32_t rx_core, wt_core, tx_core, rx_port, tx_port, rx_queue, tx_queue;
    struct rte_ring *rx_ring;
    struct rte_ring *tx_ring;
    struct rte_sched_port *sched_port;
    struct rte_mempool *mbuf_pool;

    struct thread_conf rx_thread;
    struct thread_conf wt_thread;
    struct thread_conf tx_thread;
};


extern struct flow_conf qos_conf[];
extern uint32_t nb_pfc;
extern uint8_t interactive;
extern int mp_size;
extern const char *cfg_profile;

extern struct ring_conf ring_conf;
extern struct burst_conf burst_conf;

void prompt(void);

void app_rx_thread(struct thread_conf **qconf);
void app_tx_thread(struct thread_conf **qconf);
void app_worker_thread(struct thread_conf **qconf);
void app_mixed_thread(struct thread_conf **qconf);

int app_parse_args(int argc, char **argv);
int app_init(void);

#endif //DKDP_QOS_MAIN_H
