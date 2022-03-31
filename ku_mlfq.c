#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

/*
    각자의 process를 구조체 내부에 정의하여 관리.
    Linked-List로 구성되어 Process의 timesharing을 가능게 함.

    next : 다음 Process
    pid : 관리되고 있는 Process id
    use_cpu_time : 현재 priority에서 cpu사용 시간
    priority : 현재 프로세스의 priority
*/
typedef struct process {
    struct process *next;
    pid_t pid;
    int use_cpu_time;
    int priority;
} Process;

/*
    각 Process들을 관리하고있는 Linked list
    Priority에 따라 3가지가 존재.
*/
typedef struct list {
    Process* head;
    Process* tail;
} List;

/*
    10s 이후 모든 Process Boost Priority
    use_cpu_time 2s이후 priority step down
*/
const int BOOST_TIME = 10;
const int STEPDOWN_TIME = 2;

List* queue[3]; // priority에 따른 3개 Linked-List : priority 2, 1 ,0 순
Process* running; // current running process
int timePast = 0; // all running time
int timeSliceToRun;

/*
    create Process 이후 time scheduling을 위해
    가장 높은 priority(3)에 process 삽입을 위한 함수
*/
void listInit();
void initInsert_process(pid_t pid);

void what_OS_do();

void os_timer_handler(); // os에서 time Slice (1s) 호출 handler

/*
    현재 실행되는 Process (running) 삽입
    MAINTAIN => 기존 Priority 유지
    STEPDOWN => Priority step down

    void insert_process(Process*, int)와 함께 사용
    args :
        0 : Process* => current running Process
        1 : int => flag : MAINTAIN, STEPDOWN
*/
const int MAINTAIN = 0;
const int STEPDOWN = 1;
void insert_process(Process* running, int flag);
Process* extract_process();
void priority_boost();
void quick_sort(Process* arr[], int start, int end); // priority boost 내부에서 사용하는 sort

int main(int argc, char* argv[]) {
/*
----------------- Check the all Argument set ------------------------
*/
    if (argc != 3) {
        fprintf(stderr, "It's not fint in number of Arguments.\n");
        exit(1);
    }

    if (atoi(argv[1]) < 1 || atoi(argv[1]) > 26) {
        fprintf(stderr, "Argument is not in Correct Range.\n");
        exit(1);
    }
/*
----------------------------------------------------------------------
*/

    int process_count = atoi(argv[1]);
    timeSliceToRun = atoi(argv[2]);
    pid_t pid;
    int i;

    listInit();
    for (i = 0; i < process_count; i++) {
        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork Error: errno.%d\n", errno);
            exit(1);
        } else if (pid > 0) { // parent
            initInsert_process(pid);
        } else { // child

            char c[2];
            c[0] = 'A' + i;
            c[1] = '\0';
            if (execl("./ku_app", "./ku_app", c, (char*) 0) == -1) {
                fprintf(stderr, "execl Invalid: Errno.%d\n", errno);
                exit(1);
            }

        }
    }

    // ----- Parent Process Do (OS)
    sleep(5);
    what_OS_do();

    while (1);
    return 0;
}

void initInsert_process(pid_t pid) {
    Process* ps = (Process*) malloc(sizeof(Process));
    ps->next = NULL;
    ps->pid = pid;
    ps->priority = 3;
    ps->use_cpu_time = 0;

    if (queue[2]->head == NULL && queue[2]->tail == NULL) {
        queue[2]->head = queue[2]->tail = ps;
    } else {
        queue[2]->tail->next = ps;
        queue[2]->tail = ps;
    }
}

void listInit() {
    int i = 0;
    for (i = 0; i < 3; i++) {
        queue[i] = (List*) malloc(sizeof(List));
        queue[i]->head = queue[i]->tail = NULL;
    }
}

void what_OS_do() {
    // 현재 priority가 가장 높은 queue 앞에 있는거 가져와서 실행
    running = extract_process();
    kill(running->pid, SIGCONT);

    struct sigaction ps_check_sig;
    sigemptyset(&ps_check_sig.sa_mask);
    ps_check_sig.sa_flags = 0;
    ps_check_sig.sa_handler = os_timer_handler;
    sigaction(SIGALRM, &ps_check_sig, NULL);

    // 1s time slice timer
    struct itimerval ch_ps_timer;
    ch_ps_timer.it_interval.tv_sec = 1;
    ch_ps_timer.it_interval.tv_usec = 0;
    ch_ps_timer.it_value.tv_sec = 1;
    ch_ps_timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &ch_ps_timer, NULL);
}

void os_timer_handler() {
    kill(running->pid, SIGSTOP); // 돌고 있는 Process 중지
    running->use_cpu_time += 1; // 1초 마다 실행되므로 1초 추가
    timePast += 1; // 총 시작한 시간으로부터 1초 추가

    if (timePast == timeSliceToRun) {
        kill(0, SIGINT);
    }

    if (timePast % BOOST_TIME == 0) {
        insert_process(running, MAINTAIN);
        priority_boost();
    } else if (running->use_cpu_time == STEPDOWN_TIME)
        insert_process(running, STEPDOWN);
    else
        insert_process(running, MAINTAIN);

/* ---------------------- checked ----------------------------- */
/*               2 초마다 priority 가 내려가는지 확인                 */
    // int i = 0;
    // for (i = 2; i >= 0; i--) {
    //     fprintf(stderr, "%d : ", i + 1);
    //     Process* ps = queue[i]->head;
    //     while (1) {
    //         if (ps == NULL) break;
    //         fprintf(stderr, "%d ", ps->pid);
    //         ps = ps->next;
    //     }
    //     fprintf(stderr, "\n");
    // }
/* ------------------------------------------------------------ */
    Process* focus = extract_process();
    running = focus;
    kill(running->pid, SIGCONT);
}

void insert_process(Process* ps, int flag) {
    /*
        현재 실행되는 Process (running) 삽입
        MAINTAIN => 기존 Priority 유지
        STEPDOWN => Priority step down

        void insert_process(Process*, int)와 함께 사용
        args :
            0 : Process* => current running Process
            1 : int => flag : MAINTAIN, STEPDOWN
    */
   int q_idx;
   if (flag == STEPDOWN) {
       ps->use_cpu_time = 0;
       if (ps->priority > 1) ps->priority -= 1;
   }

   q_idx = ps->priority - 1;

   if (queue[q_idx]->head == NULL && queue[q_idx]->tail == NULL) {
       queue[q_idx]->head = queue[q_idx]->tail = ps;
   } else {
       queue[q_idx]->tail->next = ps;
       queue[q_idx]->tail = ps;
   }
}

Process* extract_process() {
    Process* ps;
    int i;
    for (i = 2; i >= 0; i--) {
        if (queue[i]->head != NULL) {
            ps = queue[i]->head;

            queue[i]->head = ps->next;
            if (queue[i]->head == NULL) queue[i]->tail = NULL;
            ps->next = NULL;

            return ps;
        }
    }

    return 0;
}

void priority_boost() {

    int i = 0, arr_idx = 0;
    Process* arr[26];

    while (1) {
        Process* ex = extract_process();
        if (ex == 0) break;

        // 모든 프로세스 정보 초기화
        ex->next = NULL;
        ex->priority = 3;
        ex->use_cpu_time = 0;

        arr[arr_idx++] = ex;
    }

    // 프로세스 아이디 오름차순 정렬 -> 알파벳 순으로 정렬
    quick_sort(arr, 0, arr_idx - 1);

    // Priority3에 모두 삽입
    for (i = 0; i < arr_idx; i++) {
        insert_process(arr[i], MAINTAIN);
    }
}

void quick_sort(Process* arr[], int start, int end) {
    if (start >= end) return;

    int pivot = start;
    int i = pivot + 1;
    int j = end;
    Process* temp;

    while (i <= j) {
        while (i <= end && ((int) arr[i]->pid) <= ((int) arr[pivot]->pid)) {
            ++i;
        }
        while (j > start && ((int) arr[j]->pid) >= ((int) arr[pivot]->pid)) {
            --j;
        }

        if (i >= j) break;

        temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }

    temp = arr[j];
    arr[j] = arr[pivot];
    arr[pivot] = temp;

    quick_sort(arr, start, j - 1);
    quick_sort(arr, j + 1, end);
}