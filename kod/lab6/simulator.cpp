#include <algorithm>
#include <cstdio>
#include <deque>
#include <iostream>
#include <queue>
#include <set>
#include <string>
#include <vector>

enum Scheduler {NO_SCHED, RR_SCHED, FIFO_SCHED};

struct TaskClass{
    int T, C, prio;
    Scheduler type;

    TaskClass(int T, int C) : T(T), C(C), prio(0), type(NO_SCHED) {}
    TaskClass(int T, int C, int prio, Scheduler type) : T(T), C(C), prio(prio), type(type){}

    bool operator ==(const TaskClass &tc) const {
        return T == tc.T && C == tc.C && prio == tc.prio && type == tc.type;
    }
};

struct TaskInst{
    int id, C, d, prio;

    TaskInst(){}
    TaskInst(int id, int C, int d) : id(id), C(C), d(d) {}
    TaskInst(int id, int C, int d, int prio) : id(id), C(C), d(d), prio(prio) {}

    bool operator ==(const TaskInst &ti) const {
        return id == ti.id;
    }

    bool operator !=(const TaskInst &ti) const {
        return id != ti.id;
    }

    TaskInst& operator=(const TaskInst &ti){
        id = ti.id;
        C = ti.C;
        d = ti.d;
        prio = ti.prio;
        return *this;
    }
} NONE(-1, -1, -1);

enum EventType {END, QUANT_END, BEGIN, SIM_END};

struct Event{
    int t, id;
    EventType et;

    Event(){}
    Event(int t, int id, EventType et) : t(t), id(id), et(et) {}

    bool operator ==(const Event &e) const {
        return t == e.t && id == e.id && et == e.et;
    }
    bool operator >(const Event &e) const {
        return (t == e.t) ? (et == e.et ? id > e.id : et > e.et) : t > e.t;
    }

    void print(){
        printf("[%2d]\n", t);
        if(id == -1)
            return;
        printf("Task #%d ", id);
        switch(et){
            case END:
                printf("ENDING");
                break;
            case BEGIN:
                printf("BEGINNING");
                break;
            case QUANT_END:
                printf("QUANT EXPIRED");
                break;
            case SIM_END:
                break;
        }
        puts("");
    }
};

bool cmp_prio(const TaskInst &ti1, const TaskInst &ti2){
    if(ti1 == ti2) return false;
    return ti1.prio == ti2.prio ? ti1.d < ti2.d : ti1.prio < ti2.prio;
}

bool cmp_rprio(const TaskInst &ti1, const TaskInst &ti2){
    if(ti1 == ti2) return false;
    return ti1.prio > ti2.prio;
}

////////////////////////////////////////////////////////////////////////////////

void simulate_RMPA(int sim_duration, std::vector<TaskClass> classes){
    std::deque<TaskInst> ready;
    TaskInst active = NONE;

    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events;
    Event current;

    int lasttime = 0, elapsed;

    for(unsigned int k = 0; k < classes.size(); ++k)
        for(int i = 0; i < sim_duration; i += classes[k].T)
            events.push(Event(i, k, BEGIN));

    events.push(Event(sim_duration, -1, SIM_END));

    while(!events.empty()){
        current = events.top();
        events.pop();

        current.print();

        elapsed = current.t - lasttime;

        if(active != NONE)
            active.C -= elapsed;

        if(current.et == BEGIN){
            TaskInst new_task(current.id, classes[current.id].C, classes[current.id].T, classes[current.id].T);

            auto old_task = std::find(ready.begin(), ready.end(), new_task);

            if(old_task != ready.end()){
                printf(">> Task <#%d, %d> exceeded its limits\n", old_task->id, old_task->C);
                ready.erase(old_task);
            } else if(new_task == active){
                printf(">> Task <#%d, %d> exceeded its limits\n", active.id, active.C);
                active = NONE;
            }

            ready.push_back(new_task);
            if(active != NONE)
                ready.push_back(active);
        } else if(current.et == END){
            // don't push back active
        } else if(current.et == SIM_END)
            puts("Simulation finished.");

        std::sort(ready.begin(), ready.end(), cmp_prio);

        if(!ready.empty()){
            active = ready.front();
            ready.pop_front();
        } else
            active = NONE;

        if(active == NONE)
            printf("No active task\n");
        else
            printf("Active task: <#%d, %d>\n", active.id, active.C);

        printf("Ready tasks: ");
        for(auto task : ready)
            printf("<#%d, %d>, ", task.id, task.C);
        printf("\n");

        if(active != NONE && active.C <= events.top().t - current.t)
            events.push(Event(current.t + active.C, active.id, END));

        lasttime = current.t;
        puts("");
    }
}

void simulate_EDF(int sim_duration, std::vector<TaskClass> classes){
    std::deque<TaskInst> ready;
    TaskInst active = NONE;

    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events;
    Event current;

    int lasttime = 0, elapsed;

    for(unsigned int k = 0; k < classes.size(); ++k)
        for(int i = 0; i < sim_duration; i += classes[k].T)
            events.push(Event(i, k, BEGIN));

    events.push(Event(sim_duration, -1, SIM_END));

    while(!events.empty()){
        current = events.top();
        events.pop();

        if(current.t > sim_duration)
            break;

        current.print();

        elapsed = current.t - lasttime;

        if(active != NONE){
            active.d -= elapsed;
            active.prio = active.d;
            active.C -= elapsed;
        }

        for(auto &task : ready){
            task.d -= elapsed;
            task.prio = task.d;
        }

        if(current.et == BEGIN){
            TaskInst new_task(current.id, classes[current.id].C, classes[current.id].T, classes[current.id].T);

            auto old_task = std::find(ready.begin(), ready.end(), new_task);

            if(old_task != ready.end()){
                printf(">> Task <#%d, %d, %d> exceeded its limits\n", old_task->id, old_task->C, old_task->d);
                ready.erase(old_task);
            } else if(new_task == active){
                printf(">> Task <#%d, %d, %d> exceeded its limits\n", active.id, active.C, active.d);
                active = NONE;
            }

            ready.push_back(new_task);
            if(active != NONE)
                ready.push_back(active);

        } else if(current.et == END){
            // don't push back active
        } else if(current.et == SIM_END)
            puts("Simulation finished.");

        std::sort(ready.begin(), ready.end(), cmp_prio);

        if(!ready.empty()){
            active = ready.front();
            ready.pop_front();
        } else
            active = NONE;

        if(active == NONE)
            printf("No active task\n");
        else
            printf("Active task: <#%d, %d, %d>\n", active.id, active.C, active.d);

        printf("Ready tasks: ");
        for(auto task : ready)
            printf("<#%d, %d, %d>, ", task.id, task.C, task.d);
        printf("\n");

        if(active != NONE && active.C <= events.top().t - current.t)
            events.push(Event(current.t + active.C, active.id, END));

        lasttime = current.t;
        puts("");
    }
}

void simulate_LLF(int sim_duration, int sim_step, std::vector<TaskClass> classes){
    std::deque<TaskInst> ready;
    TaskInst active = NONE;

    for(int t = 0; t < sim_duration; t += sim_step){

        printf("[%2d]\n", t);

        if(active != NONE){
            active.C -= sim_step;
            if(active.C <= 0){
                printf("Task <#%d, %d, %d> completed.\n", active.id, active.prio, active.C);
                active = NONE;
            }
        }

        for(auto &task : ready)
            if(task != active)
                task.prio -= sim_step;

        for(unsigned int k = 0; k < classes.size(); ++k)
            if(t % classes[k].T == 0){
                // create new task instance
                TaskInst new_task(k, classes[k].C, classes[k].T, classes[k].T - classes[k].C);

                auto old_task = std::find(ready.begin(), ready.end(), new_task);

                if(old_task != ready.end()){
                    printf(">> Task <#%d, %d, %d> exceeded its limits\n", old_task->id, old_task->prio, old_task->C);
                    ready.erase(old_task);
                } else if(new_task == active){
                    printf(">> Task <#%d, %d, %d> exceeded its limits\n", active.id, active.prio, active.C);
                    active = NONE;
                }

                ready.push_back(new_task);
            }

        if(active != NONE)
            ready.push_back(active);

        std::sort(ready.begin(), ready.end(), cmp_prio);

        if(!ready.empty()){
            active = ready.front();
            ready.pop_front();
        } else
            active = NONE;

        if(active != NONE)
            printf("Active task: <#%d, %d, %d>\n", active.id, active.prio, active.C);
        else
            puts("No active task");

        printf("Ready tasks: ");
        for(auto task : ready)
            printf("<#%d, %d, %d>, ", task.id, task.prio, task.C);
        printf("\n");

        puts("");
    }
}

void simulate_SCHED(int sim_duration, int quant, std::vector<TaskClass> classes){
    std::deque<TaskInst> ready;
    TaskInst active = NONE;

    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> events;
    Event current;

    int lasttime = 0, elapsed;

    for(unsigned int k = 0; k < classes.size(); ++k)
        for(int i = 0; i < sim_duration; i += classes[k].T)
            events.push(Event(i, k, BEGIN));

    events.push(Event(sim_duration, -1, SIM_END));

    while(!events.empty()){
        current = events.top();
        events.pop();

        if(current.t > sim_duration)
            break;

        current.print();

        elapsed = current.t - lasttime;

        if(active != NONE)
            active.C -= elapsed;

        if(current.et == BEGIN){
            TaskInst new_task(current.id, classes[current.id].C, classes[current.id].T, classes[current.id].prio);
            auto old_task = std::find(ready.begin(), ready.end(), new_task);

            if(old_task != ready.end()){
                printf(">> Task <#%d, %d, %d, %s> exceeded its limits\n", old_task->id, old_task->prio, old_task->C, classes[old_task->id].type == FIFO_SCHED ? "FIFO" : "RR");
                ready.erase(old_task);
            } else if(new_task == active){
                printf(">> Task <#%d, %d, %d, %s> exceeded its limits\n", active.id, active.prio, active.C, classes[active.id].type == FIFO_SCHED ? "FIFO" : "RR");
                active = NONE;
            }

            if(new_task != NONE)
                ready.push_back(new_task);

            if(active != NONE)
                ready.push_front(active);

        } else if(current.et == END){
            // dont return active to the queue
        } else if(current.et == QUANT_END){
            ready.push_back(active);
        } else if(current.et == SIM_END)
            puts("Simulation finished.");

        std::stable_sort(ready.begin(), ready.end(), cmp_rprio);

        if(!ready.empty()){
            active = ready.front();
            ready.pop_front();
        } else
            active = NONE;

        if(active != NONE)
            printf("Active task: <#%d, %d, %d, %s>\n", active.id, active.prio, active.C, classes[active.id].type == FIFO_SCHED ? "FIFO" : "RR");
        else
            printf("No active task\n");

        printf("Ready tasks: ");
        for(auto task : ready)
            printf("<#%d, %d, %d, %s>, ", task.id, task.prio, task.C, classes[task.id].type == FIFO_SCHED ? "FIFO" : "RR");
            printf("\n");

        if(active != NONE){
            if(classes[active.id].type == RR_SCHED && active.C > quant && quant <= events.top().t - current.t){
                //printf(">> Adding QUANT_END for #%d at %d\n", active.id, current.t + quant);
                events.push(Event(current.t + quant, active.id, QUANT_END));
            } else if(active.C <= events.top().t - current.t)
                events.push(Event(current.t + active.C, active.id, END));
        }

        puts("");

        lasttime = current.t;
    }
}

////////////////////////////////////////////////////////////////////////////////

int main(){
    int sim_duration;
    std::string scheduling;
    std::vector<TaskClass> classes;

    std::cin >> scheduling >> sim_duration;
    if(scheduling == "RMPA"){
        int T, C;

        while(scanf("%d%d", &T, &C) > 0)
            classes.push_back(TaskClass(T, C));

        simulate_RMPA(sim_duration, classes);
    } else if(scheduling == "EDF"){
        int T, C;

        while(scanf("%d%d", &T, &C) > 0)
            classes.push_back(TaskClass(T, C));

        simulate_EDF(sim_duration, classes);
    } else if(scheduling == "LLF"){
        int T, C, sim_step;

        std::cin >> sim_step;
        while(scanf("%d%d", &T, &C) > 0)
            classes.push_back(TaskClass(T, C));

        simulate_LLF(sim_duration, sim_step, classes);
    } else if(scheduling == "SCHED"){
        int T, C, prio, quant;
        char type[8];

        std::cin >> quant;
        while(scanf("%d%d%d%s", &T, &C, &prio, type) > 0)
            classes.push_back(TaskClass(T, C, prio, type[0] == 'F' ? FIFO_SCHED : RR_SCHED));

        simulate_SCHED(sim_duration, quant, classes);
    }

    return 0;
}