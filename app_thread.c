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

static inline void
app_send_burst(struct thread_conf *qconf) {
    uint32_t n = qconf->n_mbufs, ret;
    struct rte_mbuf **mbufs = (struct rte_mbuf **)qconf->m_table;

    do {
        ret = rte_eth_tx_burst(qconf->tx_port, qconf->tx_queue, mbufs, (uint16_t)n);
        n -= ret;
        mbufs = (struct rte_mbuf **)&mbufs[ret];
    } while (n);

}

static void
app_send_packets(struct thread_conf *qconf, struct rte_mbuf **mbufs, uint32_t nb_pkt) {

    uint32_t i, len = qconf->n_mbufs;
    for(i = 0; i < nb_pkt; ++i) {
        qconf->m_table[len] = mbufs[i];
        ++len;
        if (unlikely(len == burst_conf.tx_burst)) {
            qconf->n_mbufs = len;
            app_send_burst(qconf);
            len = 0;
        }
    }
    qconf->n_mbufs = len;

}

void app_rx_thread(struct thread_conf **qconf) {

    uint32_t i, nb_rx;
    int conf_idx = 0;
    struct thread_conf *conf;
    struct rte_mbuf *rx_mbufs[burst_conf.rx_burst];

    uint32_t subport, pipe, traffic_class, queue, color;

    while ((conf = qconf[conf_idx])) {
        nb_rx = rte_eth_rx_burst(conf->rx_port, conf->rx_queue, rx_mbufs, burst_conf.rx_burst);

        if (likely(nb_rx != 0)) {
            APP_STATS_ADD(conf->stat.nb_rx, nb_rx);
            for (i = 0; i < nb_rx; ++i) {
                get_pkt_sched(rx_mbufs[i], &subport, &pipe, &traffic_class, &queue, &color);
                rte_sched_port_pkt_write(conf->sched_port, rx_mbufs[i], subport, pipe, traffic_class, queue, (enum rte_color) color);
            }

            if (unlikely (rte_ring_sp_enqueue_bulk(conf->rx_ring, (void **)rx_mbufs, nb_rx, NULL) == 0)) {
                for(i = 0; i < nb_rx; ++i) {
                    rte_pktmbuf_free(rx_mbufs[i]);
                    APP_STATS_ADD(conf->stat.nb_drop, 1);
                }
            }
        }
        ++conf_idx;
        if (qconf[conf_idx] == NULL)
            conf_idx = 0;
    }
}
void app_tx_thread(struct thread_conf **qconf) {
    int conf_idx = 0, ret;
    struct thread_conf *conf;
    struct rte_mbuf *tx_mbufs[burst_conf.qos_dequeue];
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

    while ((conf = qconf[conf_idx])) {
        ret = rte_ring_sc_dequeue_bulk(conf->tx_ring, (void **)tx_mbufs, burst_conf.qos_dequeue, NULL);
        if (likely(ret != 0)) {
            app_send_packets(conf, tx_mbufs, burst_conf.qos_dequeue);
            conf->counter = 0;
        }
        ++conf->counter;

        if (unlikely(conf->n_mbufs > drain_tsc)) {
            if (conf->n_mbufs != 0) {
                app_send_burst(conf);
                conf->n_mbufs = 0;
            }
            conf->counter = 0;
        }
        ++conf_idx;

        if (qconf[conf_idx] == NULL)
            conf_idx = 0;
    }
}
void app_worker_thread(struct thread_conf **qconf) {
    int conf_idx = 0;
    struct thread_conf *conf;
    struct rte_mbuf *mbufs[burst_conf.ring_burst];

    while ((conf = qconf[conf_idx])) {
        uint32_t nb_pkt = rte_ring_sc_dequeue_burst(conf->rx_ring, (void **)mbufs, burst_conf.ring_burst, NULL);
        if (likely(nb_pkt)) {
            int nb_sent = rte_sched_port_enqueue(conf->sched_port, mbufs, nb_pkt);
            APP_STATS_ADD(conf->stat.nb_drop, nb_pkt - nb_sent);
            APP_STATS_ADD(conf->stat.nb_rx, nb_pkt);
        }
        nb_pkt = rte_sched_port_dequeue(conf->sched_port, mbufs, burst_conf.qos_dequeue);
        if (likely(nb_pkt > 0))
            while (rte_ring_sp_enqueue_bulk(conf->tx_ring, (void **)mbufs, nb_pkt, NULL) == 0);

        ++conf_idx;
        if (qconf[conf_idx] == NULL)
            conf_idx = 0;
    }
}
void app_mixed_thread(struct thread_conf **qconf) {
    int conf_idx = 0;
    struct thread_conf *conf;
    struct rte_mbuf *tx_mbufs[burst_conf.ring_burst];
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

    while ((conf = qconf[conf_idx])) {
        uint32_t nb_pkt = rte_ring_sc_dequeue_burst(conf->rx_ring, (void **)mbufs, burst_conf.ring_burst, NULL);
        if (likely (nb_pkt)) {
            int nb_sent = rte_sched_port_enqueue(conf->sched_port, mbufs, nb_pkt);
            APP_STATS_ADD(conf->stat.nb_drop, nb_pkt - nb_sent);
            APP_STATS_ADD(conf->stat.nb_rx, nb_pkt);
        }
        nb_pkt = rte_sched_port_dequeue(conf->sched_port, mbufs, burst_conf.qos_dequeue);
        if (likely (nb_pkt > 0)) {
            app_send_packets(conf, mbufs, nb_pkt);
            conf->counter = 0;
        }
        ++conf->counter;

        if (unlikely (conf->counter > drain_tsc)) {
            if (conf->n_mbufs != 0) {
                app_send_burst(conf);
                conf->n_mbufs = 0;
            }
            conf->counter = 0;
        }
        ++conf_idx;
        if(qconf[conf_idx] == NULL)
            conf_idx = 0;
    }

}
