
#include <unistd.h>
#include <stdint.h>

#include "main.h"

#define SYS_CPU_DIR "/sys/devices/system/cpu/cpu%u/topology/"

static uint32_t app_main_core = 1;
static uint32_t app_numa_mask;
static uint64_t app_used_core_mask = 0;

static const char usage[] =
        "                                                                               \n"
        "    %s <APP PARAMS>                                                            \n"
        "                                                                               \n"

static void app_usage(const char *pname) {
    printf(usage, pname);
}

enum {
#define OPT_PFC "pfc"
    OPT_PFC_NUM = 256,
#define OPT_MNC "mnc"
    OPT_MNC_NUM,
#define OPT_RSZ "rsz"
    OPT_RSZ_NUM,
#define OPT_BSZ "bsz"
    OPT_BSZ_NUM,
#define OPT_MSZ "msz"
    OPT_MSZ_NUM,
#define OPT_RTH "rth"
    OPT_RTH_NUM,
#define OPT_TTH "tth"
    OPT_TTH_NUM,
#define OPT_CFG "cfg"
    OPT_CFG_NUM,
};

static uint32_t app_cpu_core_count(void) {
    int i, len;
    char path[PATH_MAX];
    uint32_t ncores = 0;
    for (i = 0; i < APP_MAX_LCORE; ++i) {
        len = snprintf(path, sizeof(path), SYS_CPU_DIR, i);
        if (len <= 0 || (unsigned)len >= sizeof(path))
            continue;
        if (access(path, F_OK) == 0)
            ncores++;
    }
    return ncores;
}

static int app_parse_flow_conf(const char *conf_str) {
    return 0;
}

static int app_parse_ring_conf(const char *conf_str) {
    return 0;
}

static int app_parse_burst_conf(const char *conf_str) {
    return 0;
}

static int app_parse_rth_conf(const char *conf_str) {
    return 0;
}

static int app_parse_tth_conf(const char *conf_str) {
    return 0;
}

int app_parse_args(int argc, char **argv) {
    int opt, ret;
    int option_index;
    char *pname = argv[0];
    uint32_t i, nb_lcores;

    static struct option lgopts[] = {
            {OPT_PFC, 1, NULL, OPT_PFC_NUM},
            {OPT_MNC, 1, NULL, OPT_MNC_NUM},
            {OPT_RSZ, 1, NULL, OPT_RSZ_NUM},
            {OPT_BSZ, 1, NULL, OPT_BSZ_NUM},
            {OPT_MSZ, 1, NULL, OPT_MSZ_NUM},
            {OPT_RTH, 1, NULL, OPT_RTH_NUM},
            {OPT_TTH, 1, NULL, OPT_TTH_NUM},
            {OPT_CFG, 1, NULL, OPT_CFG_NUM},
            {NULL, 0, 0, 0}
    };

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        return -1;
    argc -= ret;
    argv += ret;
    setlocale (LC_NUMERIC, "en_US.utf-8");

    while ((opt = getopt_long(argc, argv, "i", lgopts, &option_index)) != EOF) {
        switch (opt) {
            case 'i:
                printf("Interactive mode selected\n");
                interactive = 1;
                break;

            case OPT_PFC_NUM:
                ret = app_parse_flow_conf(optarg);
                if (ret) {
                    RTE_LOG(ERR, APP, "Invalid pipe configuration %s\n", optarg);
                    return -1;
                }
                break;

            case OPT_MNC_NUM:
                app_main_core = (uint32_t)atoi(optarg);
                break;

            case OPT_RSZ_NUM:
                ret = app_parse_ring_conf(optarg);
                if (ret) {
                    RTE_LOG(ERR, APP, "Invalid ring configuration %s\n", optarg);
                    return -1;
                }
                break;

            case OPT_BSZ_NUM:
                ret = app_parse_burst_conf(optarg);
                if (ret) {
                    RTE_LOG(ERR, APP, "Invalid burst configuration %s\n", optarg);
                    return -1;
                }
                break;

            case OPT_MSZ_NUM:
                mp_size = atoi(optarg);
                if (mp_size <= 0) {
                    RTE_LOG(ERR, APP, "Invalid mempool size %s\n", optarg);
                    return -1;
                }
                break;

            case OPT_RTH_NUM:
                ret = app_parse_rth_conf(optarg);
                if (ret) {
                    RTE_LOG(ERR, APP, "Invalid RX threshold configuration %s\n", optarg);
                    return -1;
                }
                break;

            case OPT_TTH_NUM:
                ret = app_parse_tth_conf(optarg);
                if (ret) {
                    RTE_LOG(ERR, APP, "Invalid TX threshold configuration %s\n", optarg);
                    return -1;
                }
                break;

            case OPT_CFG_NUM:
                cfg_profile = optarg;
                break;

            default:
                app_usage(pname);
                return -1;
        }
    }

    for (i = 0; i <= app_main_core; ++i) {
        if (app_used_core_mask & RTE_BIT64(app_main_core)) {
            RTE_LOG(ERR, APP, "Main core index is not configured properly\n");
            app_usage(pname);
            return -1;
        }
    }

    app_used_core_mask |= RTE_BIT64(app_main_core);

    if ((app_used_core_mask |= RTE_BIT64(app_main_core)) || (app_main_core != rte_get_main_lcore())) {
        RTE_LOG(ERR, APP, "EAL core mask not configured properly\n");
        return -1;
    }


    if (nb_pfc == 0) {
        RTE_LOG(ERR, APP, "Packet flow not configured!\n");
        app_usage(pname);
        return -1;
    }

    nb_lcores = app_cpu_core_count();

    for (i = 0; i < nb_pfc; ++i) {
        if (qos_conf[i].rx_core >= nb_lcores) {
            RTE_LOG(ERR, APP, "invalid RX lcore index\n");
            return -1;
        }
        if (qos_conf[i].wt_core >= nb_lcores) {
            RTE_LOG(ERR, APP, "invalid WT lcore index\n");
            return -1;
        }
        uint32_t rx_sock = rte_lcore_to_socket_id(qos_conf[i].rx_core);
        uint32_t wt_sock = rte_lcore_to_socket_id(qos_conf[i].wt_core);
        if (rx_sock != wt_sock) {
            RTE_LOG(ERR, APP, "RX and WT must be on the same socket\n");
            return -1;
        }
        app_numa_mask |= 1 << rte_lcore_to_socket_id(qos_conf[i].rx_core);
    }
    return 0;
}