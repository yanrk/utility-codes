#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "network_check.h"

#define USE_BASH            1

#define TEST_DOMAIN         "speedtest.qingjiaocloud.com"
#define TEST_DOWNLOAD_URL   "speedtest.qingjiaocloud.com/10MB.bin"

static FILE * my_popen(const char * command, const char * modes, pid_t * child_pid/* = NULL*/)
{
    int pipe_fd[2] = { -1, -1 };
    if (pipe(pipe_fd) < 0)
    {
        return NULL;
    }

    pid_t cpid = fork();
    if (cpid < -1)
    {
        return NULL;
    }

    int reader = (0 == strcmp("r", modes) ? 0 : 1);
    int writer = (1 - reader);

    if (0 == cpid)
    {
        close(pipe_fd[reader]);
        dup2(pipe_fd[writer], STDOUT_FILENO);
        close(pipe_fd[writer]);
#ifdef USE_BASH
        execl("/bin/bash", "bash", "-c", command, NULL);
#else
        execl("/bin/sh", "sh", "-c", command, NULL);
#endif // USE_BASH
        _exit(0);
    }
    else
    {
        if (NULL != child_pid)
        {
            *child_pid = cpid;
        }

        close(pipe_fd[writer]);

        FILE * file = fdopen(pipe_fd[reader], modes);
        if (NULL == file)
        {
            close(pipe_fd[reader]);
            return NULL;
        }

        return file;
    }

    return NULL;
}

int my_pclose(FILE * file)
{
    return fclose(file);
}

static int do_get_address(char host_ip[16], char host_mac[18], char host_mask[16])
{
    int result = network_check_exception;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        return result;
    }

    char buffer[1024] = { 0x0 };
    struct ifconf ifc = { 0x0 };
    ifc.ifc_buf = buffer;
    ifc.ifc_len = sizeof(buffer);
    if (0 != ioctl(sock, SIOCGIFCONF, &ifc))
    {
        close(sock);
        return result;
    }

    result = network_check_failure;

    for (size_t index = 0, count = ifc.ifc_len / sizeof(struct ifreq); index < count; ++index)
    {
        struct ifreq ifr = { 0x0 };
        strncpy(ifr.ifr_name, ifc.ifc_req[index].ifr_name, sizeof(ifr.ifr_name) - 1);

        if (0 != ioctl(sock, SIOCGIFADDR, &ifr))
        {
            continue;
        }

        struct sockaddr_in * ip_addr = (struct sockaddr_in *)(&ifr.ifr_addr);
        inet_ntop(AF_INET, &ip_addr->sin_addr, host_ip, 15);
        if (0 == strcmp("0.0.0.0", host_ip) || 0 == strcmp("127.0.0.1", host_ip))
        {
            continue;
        }

        if (0 != ioctl(sock, SIOCGIFHWADDR, &ifr))
        {
            continue;
        }

        unsigned char * mac = (unsigned char *)(ifr.ifr_hwaddr.sa_data);
        snprintf(host_mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        if (0 == strcmp("00:00:00:00:00:00", host_mac))
        {
            continue;
        }

        if (0 != ioctl(sock, SIOCGIFNETMASK, &ifr))
        {
            continue;
        }

        struct sockaddr_in * mask_addr = (struct sockaddr_in *)(&ifr.ifr_netmask);
        inet_ntop(AF_INET, &mask_addr->sin_addr, host_mask, 15);

        result = network_check_success;
    }

    close(sock);

    return result;
}

static int do_get_gateway(char gateway_ip[16], pid_t * child_pid/* = NULL*/)
{
    int result = network_check_exception;

    FILE * file = my_popen("ip route | grep default", "r", child_pid);
    if (NULL == file)
    {
        return result;
    }

    result = network_check_failure;

    char line[128] = { 0x0 };
    while (NULL != fgets(line, sizeof(line) - 1, file) && network_check_success != result)
    {
        const char * header = line;
        while (NULL != header)
        {
            const char * ip_beg = strchr(header, '.');
            if (NULL == ip_beg)
            {
                break;
            }

            while (ip_beg > header && *(ip_beg - 1) != ' ')
            {
                --ip_beg;
            }

            const char * ip_end = strchr(ip_beg, ' ');

            int sym_count = 0;
            int dot_count = 0;
            const char* check = ip_beg;
            while (ip_end != check && 0x0 != *check)
            {
                if ('.' == *check)
                {
                    ++dot_count;
                }
                else if (!isdigit(*check))
                {
                    dot_count = 0;
                    break;
                }
                ++check;
                ++sym_count;
            }

            if (3 == dot_count && sym_count < 16)
            {
                if (NULL == ip_end)
                {
                    strcpy(gateway_ip, ip_beg);
                }
                else
                {
                    strncpy(gateway_ip, ip_beg, ip_end - ip_beg);
                }

                result = network_check_success;
            }

            header = ip_end;
        }
    }

    my_pclose(file);

    return result;
}

static int do_get_dns(char dns_ip[16], pid_t * child_pid/* = NULL*/)
{
    int result = network_check_exception;

    FILE * file = fopen("/etc/resolv.conf", "r");
    if (NULL == file)
    {
        return result;
    }

    result = network_check_failure;

    char line[128] = { 0x0 };
    while (NULL != fgets(line, sizeof(line) - 1, file) && network_check_success != result)
    {
        const char * pos_beg = strstr(line, "nameserver");
        if (NULL == pos_beg)
        {
            continue;
        }

        pos_beg = strchr(pos_beg, ' ');
        if (NULL == pos_beg)
        {
            continue;
        }

        pos_beg += 1;

        const char * pos_end = pos_beg;
        while ('.' == *pos_end || isdigit(*pos_end))
        {
            ++pos_end;
        }

        if (pos_end - pos_beg > 15)
        {
            continue;
        }

        strncpy(dns_ip, pos_beg, pos_end - pos_beg);

        result = network_check_success;
    }

    fclose(file);

    return result;
}

static int do_ping_server(const char * ping_ip, int * loss, double * delay, pid_t * child_pid/* = NULL*/)
{
    int result = network_check_exception;

    char command[64] = { 0x0 };
    snprintf(command, sizeof(command) - 1, "ping -c 4 %s", ping_ip);

    FILE * file = my_popen(command, "r", child_pid);
    if (NULL == file)
    {
        return result;
    }

    result = network_check_failure;

    int step = 0;
    char line[128] = { 0x0 };
    while (NULL != fgets(line, sizeof(line) - 1, file) && network_check_success != result)
    {
        if (0 == step)
        {
            if (NULL != strstr(line, "---"))
            {
                step = 1;
            }
            continue;
        }
        else if (1 == step)
        {
            const char * per_end = strchr(line, '%');
            if (NULL == per_end)
            {
                continue;
            }

            const char * per_beg = per_end;
            while (per_beg > line && isdigit(*(per_beg - 1)))
            {
                --per_beg;
            }

            if (per_end - per_beg >= 1 && per_end - per_beg <= 3)
            {
                char temp[4] = { 0x0 };
                strncpy(temp, per_beg, per_end - per_beg);
                int percent = atoi(temp);
                if (percent <= 100)
                {
                    *loss = percent;
                    step = 2;
                }
            }
        }
        else if (2 == step)
        {
            const char * equal_sign = strchr(line, '=');
            if (NULL == equal_sign)
            {
                continue;
            }

            const char * first_slash = strchr(equal_sign, '/');
            if (NULL == first_slash)
            {
                continue;
            }

            const char * second_slash = strchr(first_slash + 1, '/');
            if (NULL == second_slash)
            {
                continue;
            }

            if (second_slash - first_slash > 1 && second_slash - first_slash < 32)
            {
                char average[32] = { 0x0 };
                strncpy(average, first_slash + 1, second_slash - first_slash - 1);
                *delay = atof(average);
                step = 3;
                result = network_check_success;
            }
        }
    }

    my_pclose(file);

    return result;
}

static int do_parse_domain()
{
    int result = network_check_exception;

    const char * domain = TEST_DOMAIN;

    struct addrinfo addr_temp;
    memset(&addr_temp, 0x00, sizeof(addr_temp));
    addr_temp.ai_family = AF_UNSPEC;
    addr_temp.ai_socktype = SOCK_STREAM;

    struct addrinfo * addr_info = NULL;
    if (0 != getaddrinfo(domain, NULL, &addr_temp, &addr_info))
    {
        return result;
    }

    result = network_check_failure;

    struct addrinfo * addr_dup = addr_info;

    for (; NULL != addr_info; addr_info = addr_info->ai_next)
    {
        if (AF_INET != addr_info->ai_family)
        {
            continue;
        }

        if (SOCK_STREAM != addr_info->ai_socktype && SOCK_DGRAM != addr_info->ai_socktype)
        {
            continue;
        }

        char server_ip[16] = { 0x0 };
        struct sockaddr_in * ip_addr = (struct sockaddr_in *)(addr_info->ai_addr);
        inet_ntop(AF_INET, &ip_addr->sin_addr, server_ip, sizeof(server_ip) - 1);
        if (0 == strcmp("0.0.0.0", server_ip) || 0 == strcmp("127.0.0.1", server_ip))
        {
            continue;
        }

        result = network_check_success;
    }

    freeaddrinfo(addr_dup);

    return result;
}

static int do_test_download(int * speed, pid_t * child_pid/* = NULL*/)
{
    int result = network_check_exception;

    char command[256] = { "curl -o /dev/null -s -w speed:%{speed_download}\"\\n\" " TEST_DOWNLOAD_URL };

    FILE * file = my_popen(command, "r", child_pid);
    if (NULL == file)
    {
        return result;
    }

    result = network_check_failure;

    int step = 0;
    char line[128] = { 0x0 };
    while (NULL != fgets(line, sizeof(line) - 1, file) && network_check_success != result)
    {
        const char * pos_beg = strstr(line, "speed:");
        if (NULL == pos_beg)
        {
            continue;
        }
        const char * pos_end = pos_beg + 6;
        *speed = (int)(atof(pos_end));

        result = network_check_success;
    }

    my_pclose(file);

    return result;
}

enum network_check_type_t
{
    check_ip_mac,
    check_gateway,
    check_dns,
    check_lan_ping,
    check_wan_domain,
    check_wan_download,
    check_count
};

typedef struct
{
    int         check_type;
    int         result;
    pid_t       child_pid;
    char        host_ip[16];
    char        host_mac[18];
    char        host_mask[16];
    char        gateway_ip[16];
    char        dns_ip[16];
    char        ping_ip[16];
    int         ping_loss;
    double      ping_delay;
    int         download_speed;
} check_param_t;

static void * do_thread_check(void * argument)
{
    check_param_t * param = (check_param_t *)argument;
    switch (param->check_type)
    {
        case check_ip_mac:
        {
            param->result = do_get_address(param->host_ip, param->host_mac, param->host_mask);
            break;
        }
        case check_gateway:
        {
            param->result = do_get_gateway(param->gateway_ip, &param->child_pid);
            break;
        }
        case check_dns:
        {
            param->result = do_get_dns(param->dns_ip, &param->child_pid);
            break;
        }
        case check_lan_ping:
        {
            param->result = do_ping_server(param->ping_ip, &param->ping_loss, &param->ping_delay, &param->child_pid);
            break;
        }
        case check_wan_domain:
        {
            param->result = do_parse_domain();
            break;
        }
        case check_wan_download:
        {
            param->result = do_test_download(&param->download_speed, &param->child_pid);
            break;
        }
        default:
        {
            param->result = network_check_exception;
            break;
        }
    }
    return NULL;
}

static void thread_check(check_param_t * param, int timeout)
{
    pthread_t thread = 0;
    if (0 != pthread_create(&thread, NULL, do_thread_check, param))
    {
        param->result = network_check_exception;
        return;
    }

    int aborted = 0;
    time_t time_start = time(NULL);
    while (network_check_pending == param->result)
    {
        if (0 != param->child_pid && time(NULL) >= time_start + timeout)
        {
            aborted = 1;
            kill(param->child_pid, SIGKILL);
            break;
        }
        else
        {
            usleep(1000);
        }
    }

    pthread_join(thread, NULL);

    if (aborted && network_check_success != param->result)
    {
        param->result = network_check_timeout;
    }
}

int get_address(char host_ip[16], char host_mac[18], char host_mask[16], int timeout)
{
    check_param_t param = { 0 };
    param.check_type = check_ip_mac;
    param.result = network_check_pending;
    param.child_pid = 0;

    thread_check(&param, timeout);

    if (network_check_success == param.result)
    {
        strcpy(host_ip, param.host_ip);
        strcpy(host_mac, param.host_mac);
        strcpy(host_mask, param.host_mask);
    }

    return param.result;
}

int get_gateway(char gateway_ip[16], int timeout)
{
    check_param_t param = { 0 };
    param.check_type = check_gateway;
    param.result = network_check_pending;
    param.child_pid = 0;

    thread_check(&param, timeout);

    if (network_check_success == param.result)
    {
        strcpy(gateway_ip, param.gateway_ip);
    }

    return param.result;
}

int get_dns(char dns_ip[16], int timeout)
{
    check_param_t param = { 0 };
    param.check_type = check_dns;
    param.result = network_check_pending;
    param.child_pid = 0;

    thread_check(&param, timeout);

    if (network_check_success == param.result)
    {
        strcpy(dns_ip, param.dns_ip);
    }

    return param.result;
}

int ping_server(const char * ping_ip, int * loss, double * delay, int timeout)
{
    check_param_t param = { 0 };
    param.check_type = check_lan_ping;
    param.result = network_check_pending;
    param.child_pid = 0;

    strcpy(param.ping_ip, ping_ip);

    thread_check(&param, timeout);

    if (network_check_success == param.result)
    {
        *loss = param.ping_loss;
        *delay = param.ping_delay;
    }

    return param.result;
}

int parse_domain(int timeout)
{
    check_param_t param = { 0 };
    param.check_type = check_wan_domain;
    param.result = network_check_pending;
    param.child_pid = 0;

    thread_check(&param, timeout);

    return param.result;
}

int test_download(int * speed, int timeout)
{
    check_param_t param = { 0 };
    param.check_type = check_wan_download;
    param.result = network_check_pending;
    param.child_pid = 0;

    thread_check(&param, timeout);

    if (network_check_success == param.result)
    {
        *speed = param.download_speed;
    }

    return param.result;
}

const char * get_result(int result)
{
    switch (result)
    {
        case network_check_pending:
            return "pending";
        case network_check_success:
            return "success";
        case network_check_failure:
            return "failure";
        case network_check_timeout:
            return "timeout";
        case network_check_exception:
            return "exception";
        default:
            return "unknown";
    }
}

int main()
{
    char ip[16] = { 0x0 };
    char mac[18] = { 0x0 };
    char mask[16] = { 0x0 };
    int result_1 = get_address(ip, mac, mask, 2);
    char gateway_ip[16] = { 0x0 };
    int result_2 = get_gateway(gateway_ip, 2);
    char dns_ip[16] = { 0x0 };
    int result_3 = get_dns(dns_ip, 2);
    int gateway_loss = 0;
    double gateway_delay = 0;
    int result_4 = ping_server(gateway_ip, &gateway_loss, &gateway_delay, 6);
    int dns_loss = 0;
    double dns_delay = 0;
    int result_5 = ping_server(dns_ip, &dns_loss, &dns_delay, 6);
    int result_6 = parse_domain(2);
    int speed = 0;
    int result_7 = test_download(&speed, 30);
    printf("[get_address] result = (%s) (%s) (%s) (%s)\n", get_result(result_1), ip, mac, mask);
    printf("[get_gateway] result = (%s) (%s)\n", get_result(result_2), gateway_ip);
    printf("[get_dns] result = (%s) (%s)\n", get_result(result_3), dns_ip);
    printf("[ping_gateway] result = (%s) ping (%s) loss(%d%%) delay(%.3fms)\n", get_result(result_4), gateway_ip, gateway_loss, gateway_delay);
    printf("[ping_dns] result = (%s) ping (%s) loss(%d%%) delay(%.3fms)\n", get_result(result_5), dns_ip, dns_loss, dns_delay);
    printf("[parse_domain] result = (%s)\n", get_result(result_6));
    printf("[test_download] result = (%s) speed(%dB/S)\n", get_result(result_7), speed);
    return 0;
}
