#ifndef NETWORK_CHECK_H
#define NETWORK_CHECK_H


enum network_check_result_t
{
    network_check_pending,
    network_check_success,
    network_check_failure,
    network_check_timeout,
    network_check_exception
};

int get_address(char host_ip[16], char host_mac[18], char host_mask[16], int timeout/* = 2*/);
int get_gateway(char gateway_ip[16], int timeout/* = 2*/);
int get_dns(char dns_ip[16], int timeout/* = 2*/);
int ping_server(const char * ping_ip, int * loss, double * delay, int timeout/* = 6*/);
int parse_domain(int timeout/* = 2*/);
int test_download(int * speed, int timeout/* = 30*/);


#endif // NETWORK_CHECK_H
