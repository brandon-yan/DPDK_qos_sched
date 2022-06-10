#include <unistd.h>
#include <stdint.h>

#include "main.h"

#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_branch_prediction.h>

#include <rte_sched.h>

#define MAX_NAME 32

uint32_t nb_pfc;
int mp_size = NB_MBUF;

struct ring_conf ring_conf = {
        .rx_size = APP_RX_DESC_DEFAULT;
        .tx_size = APP_TX_DESC_DEFAULT;
        .ring_size = APP_RING_SIZE;
};

struct burst_conf burst_conf = {
        .rx_burst    = MAX_PKT_RX_BURST,
        .tx_burst    = MAX_PKT_TX_BURST,
        .ring_burst  = PKT_ENQUEUE,
        .qos_dequeue = PKT_DEQUEUE,
};

static int app_init_port (uint16_t portid, struct rte_mempool *mpool) {
    return 0;
}

static struct rte_sched_port *app_init_sched_port(uint32_t portid, uint32_t socketif) {
    struct rte_sched_port *port = NULL;
    return port;
}

int app_init(void) {
    uint32_t i;
    char ring_name[MAX_NAME];
    char pool_name[MAX_NAME];

    if (rte_eth_dev_count_avail() == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet port\n");
    if (app_load_cfg_profile(cfg_profile) != 0)
        rte_exit(EXIT_FAILURE, "Invalid configuration profile\n");

    for (i = 0; i < nb_pfc; ++i) {
        uint32_t socket = rte_lcore_to_socket_id(qos_conf[i].rx_core);
        struct rte_ring *ring;
        snprintf(ring_name, MAX_NAME, "ring %u %u", i, qos_conf[i].rx_core);
        ring = rte_ring_lookup(ring_name);
        if (ring == NULL) {
            qos_conf[i].rx_ring = rte_ring_create(ring_name, ring_conf.ring_size, socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
        }
        else
            qos_conf[i].rx_ring = ring;
    }

    snprintf(pool_name, MAX_NAME, "mbuf_pool %u", i);
    qos_conf[i].mbuf_pool = rte_pktmbuf_pool_create(pool_name, mp_size, burst_conf.rx_burst * 4, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_eth_dev_socket_id(qos_conf[i].rx_port))
    app_init_port(qos_conf[i].rx_port, qos_conf[i].mbuf_pool);
    app_init_port(qos_conf[i].tx_port. qos_conf[i].mbuf_pool);

    qos_conf[i].sched_port = app_init_sched_port(qos_conf[i].tx_port, socket);
}