#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include "stuck_check.h"

#pragma pack(push, 1)

typedef struct
{
    char    serial_number[32];
    char    user_key[32];
} device_info_t;

typedef struct
{
    uint32_t max_time;
    uint64_t test_time;
} delay_tester_t;

typedef struct
{
    char     file[24];
    char     func[32];
    uint32_t line;
    uint32_t max_time;
    uint64_t check_in;
    uint64_t check_out;
} action_checker_t;

#pragma pack(pop)

#define HTTP_LOG_SERVER     "https://test.mmap.com.cn/log/upload"
#define CHECKER_FILE        "/tmp/test_checker.log"
#define STUCKER_FILE        "/tmp/test_stucker.log"

#define TEST_MMAP_NAME      "test.mmap.com.cn"
#define TEST_MMAP_SIZE      4096
#define TEST_MMAP_MARK      0x12345678
#define TEST_MMAP_CREATOR   0

#define NS_OF_ONE_SECOND    1000000000ULL
#define NS_OF_ONE_MS        1000000ULL

static int                  s_running = 1;
static FILE               * s_log_file = NULL;
static int                  s_map_fd = -1;
static char               * s_map_data = NULL;
static size_t               s_map_size = 0;
static pthread_key_t        s_thread_key;
static pthread_mutex_t      s_checker_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t             s_checker_index = 0;
static const uint32_t       s_checker_limit = 50;

static uint32_t           * s_checker_pid = NULL;
static uint32_t           * s_checker_mark = NULL;
static delay_tester_t     * s_delay_tester = NULL;
static device_info_t      * s_device_info = NULL;
static uint32_t           * s_checker_count = NULL;
static action_checker_t   * s_checker_array = NULL;

#if 1 // TEST_MMAP_CREATOR

    #define RUN_LOG_DBG(fmt, ...)                       \
    if (NULL != s_log_file)                             \
    {                                                   \
        fprintf(s_log_file, fmt "\n", ##__VA_ARGS__);   \
        fflush(s_log_file);                             \
    }                                                   \

#else

    #define RUN_LOG_DBG(fmt, ...)                       \
        fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#endif // TEST_MMAP_CREATOR

static int open_log_file(int creator, int trunc)
{
    s_log_file = fopen(creator ? CHECKER_FILE : STUCKER_FILE, trunc ? "w" : "a");
    if (NULL == s_log_file)
    {
        fprintf(stderr, "create log file failed (%s)\n", strerror(errno));
        return -1;
    }
    return 0;
}

static void close_log_file()
{
    if (NULL != s_log_file)
    {
        fclose(s_log_file);
        s_log_file = NULL;
    }
}

static uint64_t get_ns_time()
{
    struct timespec ts = { 0x0 };
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ((uint64_t)(ts.tv_sec) * NS_OF_ONE_SECOND + (uint64_t)(ts.tv_nsec));
}

static void release_memory_map()
{
    if (s_map_fd < 0)
    {
        return;
    }

    RUN_LOG_DBG("release memory map begin");

    if ((char *)(-1) != s_map_data)
    {
        munmap(s_map_data, s_map_size);
    }

    close(s_map_fd);

    s_map_fd = -1;
    s_map_data = (char *)(-1);
    s_map_size = 0;

    RUN_LOG_DBG("release memory map success");
}

static int acquire_memory_map()
{
    release_memory_map();

    do
    {
        RUN_LOG_DBG("acquire memory map begin");

        s_map_fd = shm_open(TEST_MMAP_NAME, O_RDWR | O_CREAT, 0644);
        if (s_map_fd < 0)
        {
            RUN_LOG_DBG("acquire memory map failure while create share memory failed (%s)", strerror(errno));
            break;
        }

        struct stat filestat = { 0x0 };
        if (fstat(s_map_fd, &filestat) < 0)
        {
            RUN_LOG_DBG("acquire memory map failure while get share memory size failed (%s)", strerror(errno));
            break;
        }

        if ((size_t)(filestat.st_size) < TEST_MMAP_SIZE)
        {
            if (ftruncate(s_map_fd, (off_t)(TEST_MMAP_SIZE)) < 0)
            {
                RUN_LOG_DBG("acquire memory map failure while set share memory size failed (%s)", strerror(errno));
                break;
            }
        }

        s_map_data = (char *)(mmap(NULL, TEST_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, s_map_fd, 0));
        if ((char *)(-1) == s_map_data)
        {
            RUN_LOG_DBG("acquire memory map failure while get share memory data failed (%s)", strerror(errno));
            break;
        }

        s_map_size = TEST_MMAP_SIZE;

        RUN_LOG_DBG("acquire memory map success");

        return (0);
    } while (0);

    release_memory_map();

    return (-1);
}

static void close_stuck_checker()
{
#if !TEST_MMAP_CREATOR
    pthread_key_delete(s_thread_key);
#endif // TEST_MMAP_CREATOR

    release_memory_map();
}

#if TEST_MMAP_CREATOR

static int copy_log_file()
{
    FILE * src_file = fopen(CHECKER_FILE, "r");
    if (NULL == src_file)
    {
        fprintf(stderr, "copy log file failure while open src file failed (%s)\n", strerror(errno));
        return -1;
    }

    FILE * dst_file = fopen(STUCKER_FILE, "a");
    if (NULL == dst_file)
    {
        fprintf(stderr, "copy log file failure while open dst file failed (%s)\n", strerror(errno));
        fclose(src_file);
        return -1;
    }

    fputs("----- checker log begin -----\n\n\n", dst_file);

    char buffer[4096] = { 0x0 };
    while (NULL != fgets(buffer, sizeof(buffer) - 1, src_file))
    {
        fputs(buffer, dst_file);
    }

    fputs("------ checker log end ------\n\n\n", dst_file);

    fclose(src_file);
    fclose(dst_file);

    return 0;
}

static int upload_log_file()
{
    if ((char *)(-1) == s_map_data)
    {
        return -1;
    }

    close_log_file();

    if (0 != copy_log_file())
    {
        RUN_LOG_DBG("upload log file failure while copy log file");
        open_log_file(1, 0);
        return -1;
    }

    open_log_file(1, 1);

    char url[1024] = { 0x0 };
    const char * sn = s_device_info->serial_number;
    const char * user_key = s_device_info->user_key;
    sprintf(url, "curl -k -F \"file=@%s\" -X PUT -H \"Content-Type: multipart/form-data\" -H \"sn: %s\" -H \"userKey: %s\" %s", STUCKER_FILE, sn, user_key, HTTP_LOG_SERVER);
    if (0 != system(url))
    {
        RUN_LOG_DBG("upload log file failure");
        return -1;
    }
    else
    {
        RUN_LOG_DBG("upload log file success");
        return 0;
    }
}

static int check_pid_exist(pid_t pid)
{
    return kill(pid, 0);
}

static int create_stuck_checker()
{
    if (0 != acquire_memory_map())
    {
        RUN_LOG_DBG("create stuck checker failure");
        return -1;
    }

    s_checker_pid = (uint32_t *)(s_map_data);
    s_checker_mark = (uint32_t *)(s_checker_pid + 1);
    s_delay_tester = (delay_tester_t *)(s_checker_mark + 1);
    s_device_info = (device_info_t *)(s_delay_tester + 1);
    s_checker_count = (uint32_t *)(s_device_info + 1);
    s_checker_array = (action_checker_t *)(s_checker_count + 1);

    memset(s_checker_array, 0x0, sizeof(action_checker_t) * s_checker_limit);

    return 0;
}

static int stuck_checker_check()
{
    pid_t last_checker_pid = 0;

    while (s_running)
    {
        pid_t checker_pid = (pid_t)(*s_checker_pid);
        if (0 == checker_pid || last_checker_pid == checker_pid || 0 != check_pid_exist(checker_pid))
        {
            usleep(50000);
            continue;
        }

        uint32_t checker_mark = *s_checker_mark;
        if (TEST_MMAP_MARK != checker_mark)
        {
            usleep(50000);
            continue;
        }

        uint32_t delay_max_time_1 = s_delay_tester->max_time;
        uint64_t delay_test_time = s_delay_tester->test_time;
        uint32_t delay_max_time_2 = s_delay_tester->max_time;
        if (0 != delay_max_time_1 && delay_max_time_1 == delay_max_time_2 && 0 != delay_test_time)
        {
            uint64_t current = get_ns_time();
            if (current > delay_test_time + NS_OF_ONE_SECOND * delay_max_time_2)
            {
                RUN_LOG_DBG("process stream stuck: pid (%u) at delay test timeout (%us)", (uint32_t)checker_pid, delay_max_time_2);
                if (0 == kill(checker_pid, 9))
                {
                    RUN_LOG_DBG("kill process pid (%u) success", (uint32_t)checker_pid);
                }
                else
                {
                    RUN_LOG_DBG("kill process pid (%u) failure (%s)", (uint32_t)checker_pid, strerror(errno));
                }
                last_checker_pid = checker_pid;
                upload_log_file();
                continue;
            }
        }

        uint32_t checker_count = *s_checker_count;
        if (0 == checker_count || checker_count > s_checker_limit)
        {
            usleep(50000);
            continue;
        }

        for (uint32_t checker_index = 0; checker_index < checker_count; ++checker_index)
        {
            action_checker_t action_checker = { 0x0 };
            memcpy(&action_checker, &s_checker_array[checker_index], sizeof(action_checker));
            if (0 == action_checker.max_time || 0 != action_checker.check_out || action_checker.check_in == action_checker.check_out)
            {
                continue;
            }

            uint64_t current = get_ns_time();
            if (current > action_checker.check_in + NS_OF_ONE_SECOND * action_checker.max_time)
            {
                RUN_LOG_DBG("process action stuck: pid (%u) at file (%s:%s:%u) timeout (%us)", (uint32_t)checker_pid, action_checker.file, action_checker.func, action_checker.line, action_checker.max_time);
                if (0 == kill(checker_pid, 9))
                {
                    RUN_LOG_DBG("kill process pid (%u) success", (uint32_t)checker_pid);
                }
                else
                {
                    RUN_LOG_DBG("kill process pid (%u) failure (%s)", (uint32_t)checker_pid, strerror(errno));
                }
                last_checker_pid = checker_pid;
                upload_log_file();
                break;
            }
        }

        usleep(50000);
    }

    return 0;
}

int main()
{
    open_log_file(1, 1);

    if (0 != create_stuck_checker())
    {
        return 1;
    }

    stuck_checker_check();

    close_log_file();

    return 0;
}

#else

static int open_stuck_checker()
{
    pthread_key_create(&s_thread_key, NULL);

    pthread_mutex_lock(&s_checker_mutex);
    s_checker_index = 0;
    pthread_mutex_unlock(&s_checker_mutex);

    if (0 != acquire_memory_map())
    {
        RUN_LOG_DBG("open stuck checker failure");
        return -1;
    }

    s_checker_pid = (uint32_t *)(s_map_data);
    s_checker_mark = (uint32_t *)(s_checker_pid + 1);
    s_delay_tester = (delay_tester_t *)(s_checker_mark + 1);
    s_device_info = (device_info_t *)(s_delay_tester + 1);
    s_checker_count = (uint32_t *)(s_device_info + 1);
    s_checker_array = (action_checker_t *)(s_checker_count + 1);

    memset(s_device_info, 0x0, sizeof(device_info_t));
    memset(s_checker_array, 0x0, sizeof(action_checker_t) * s_checker_limit);

    s_delay_tester->max_time = 0;
    s_delay_tester->test_time = 0;

    *s_checker_mark = TEST_MMAP_MARK;
    *s_checker_count = 0;
    *s_checker_pid = (uint32_t)getpid();

    return 0;
}

static int stuck_set_device(const char * serial_number, const char * user_key)
{
    if ((char *)(-1) == s_map_data)
    {
        return -1;
    }

    if (NULL == serial_number || NULL == user_key)
    {
        return -1;
    }

    strncpy(s_device_info->serial_number, serial_number, sizeof(s_device_info->serial_number));
    strncpy(s_device_info->user_key, user_key, sizeof(s_device_info->user_key));

    return 0;
}

static int stuck_checker_in(const char * file, const char * func, int line, uint32_t timeout)
{
    if ((char *)(-1) == s_map_data)
    {
        return -1;
    }

    action_checker_t * action_checker = (action_checker_t *)pthread_getspecific(s_thread_key);
    if (NULL == action_checker)
    {
        uint32_t checker_index = 0;

        pthread_mutex_lock(&s_checker_mutex);
        checker_index = s_checker_index++;
        if (s_checker_index <= s_checker_limit)
        {
            *s_checker_count = s_checker_index;
        }
        pthread_mutex_unlock(&s_checker_mutex);

        if (checker_index >= s_checker_limit)
        {
            if (checker_index == s_checker_limit)
            {
                RUN_LOG_DBG("stuck check in failure while checker index >= %u", s_checker_limit);
            }
            return -1;
        }
        action_checker = &s_checker_array[checker_index];
        pthread_setspecific(s_thread_key, action_checker);
    }

    const char * slash = strrchr(file, '/');
    if (NULL != slash)
    {
        file = slash + 1;
    }
    strncpy(action_checker->file, file, sizeof(action_checker->file));
    strncpy(action_checker->func, func, sizeof(action_checker->func));
    action_checker->line = line;
    action_checker->max_time = timeout;
    action_checker->check_in = get_ns_time();
    action_checker->check_out = 0;

    return 0;
}

static int stuck_checker_out()
{
    if ((char *)(-1) == s_map_data)
    {
        return -1;
    }

    action_checker_t * action_checker = (action_checker_t *)pthread_getspecific(s_thread_key);
    if (NULL == action_checker)
    {
        return -1;
    }

    action_checker->check_out = get_ns_time();
    action_checker->max_time = 0;

    return 0;
}

static int stuck_delay_reset(uint32_t timeout)
{
    if ((char *)(-1) == s_map_data)
    {
        return -1;
    }

    s_delay_tester->max_time = timeout;
    s_delay_tester->test_time = get_ns_time();
}

static int stuck_delay_clear()
{
    if ((char *)(-1) == s_map_data)
    {
        return -1;
    }

    s_delay_tester->max_time = 0;
    s_delay_tester->test_time = 0;
}

static void * test_func_sleep(void * param)
{
    srand(time(NULL));

    int timeout = 25;

    while (s_running)
    {
        stuck_checker_in(__FILE__, __FUNCTION__, __LINE__, timeout);
        sleep((rand() % timeout) + 3);
        stuck_checker_out();
    }

    return NULL;
}

static void * test_func_system(void * param)
{
    srand(time(NULL));

    int timeout = 30;

    while (s_running)
    {
        stuck_checker_in(__FILE__, __FUNCTION__, __LINE__, timeout);
        if (0 == rand() % timeout)
        {
            system("read");
        }
        else
        {
            sleep(timeout / 3);
        }
        stuck_checker_out();
    }

    return NULL;
}

static void * test_func_delay(void * param)
{
    srand(time(NULL));

    int timeout = 15;

    while (s_running)
    {
        stuck_delay_reset(timeout);
        if (0 == rand() % 10)
        {
            sleep(rand() % timeout + 2);
        }
        else if (0 == rand() % 10)
        {
            stuck_delay_clear();
            sleep(timeout);
        }
        else
        {
            sleep(2);
        }
    }

    return NULL;
}

static void * test_func_exit(void * param)
{
    srand(time(NULL));

    int timeout = 5;

    while (s_running)
    {
        stuck_checker_in(__FILE__, __FUNCTION__, __LINE__, timeout);
        if (0 == rand() % 10000)
        {
            s_running = 0;
        }
        else
        {
            sleep(2);
        }
        stuck_checker_out();
    }

    return NULL;
}

int main()
{
    open_log_file(0, 1);

    if (0 != open_stuck_checker())
    {
        return 1;
    }

    if (0 != stuck_set_device("SN-ABC123XYZ-8435414875-OK", "user-key-stuck-checker"))
    {
        return 2;
    }

    pthread_t thread_sleep;
    if (0 != pthread_create(&thread_sleep, NULL, test_func_sleep, NULL))
    {
        return 3;
    }

    pthread_t thread_system;
    if (0 != pthread_create(&thread_system, NULL, test_func_system, NULL))
    {
        return 4;
    }

    pthread_t thread_delay;
    if (0 != pthread_create(&thread_delay, NULL, test_func_delay, NULL))
    {
        return 5;
    }

    pthread_t thread_exit;
    if (0 != pthread_create(&thread_exit, NULL, test_func_exit, NULL))
    {
        return 6;
    }

    pthread_join(thread_sleep, NULL);
    pthread_join(thread_system, NULL);
    pthread_join(thread_delay, NULL);
    pthread_join(thread_exit, NULL);

    close_log_file();

    return 0;
}

#endif // TEST_MMAP_CREATOR

// gcc -o stuck_check stuck_check.c -lrt -lpthread -std=gnu99
