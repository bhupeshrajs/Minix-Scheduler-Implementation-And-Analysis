/* This file contains the scheduling policy for SCHED
 *
 * The entry points are:
 *   do_noquantum:        Called on behalf of process' that run out of quantum
 *   do_start_scheduling  Request to start scheduling a proc
 *   do_stop_scheduling   Request to stop scheduling a proc
 *   do_nice		  Request to change the nice level on a proc
 *   init_scheduling      Called from main.c to set up/prepare scheduling
 */
#include "sched.h"
#include "schedproc.h"
#include <assert.h>
#include <minix/com.h>

/*Lottey Scheduling*/
#include <minix/syslib.h>

#include <machine/archtypes.h>
#include "kernel/proc.h" /* for queue constants */

static timer_t sched_timer;
static unsigned balance_timeout;

#define BALANCE_TIMEOUT	5 /* how often to balance queues in seconds */

static int schedule_process(struct schedproc * rmp, unsigned flags);
static void balance_queues(struct timer *tp);

#define SCHEDULE_CHANGE_PRIO	0x1
#define SCHEDULE_CHANGE_QUANTUM	0x2
#define SCHEDULE_CHANGE_CPU	0x4

#define SCHEDULE_CHANGE_ALL	(	\
		SCHEDULE_CHANGE_PRIO	|	\
		SCHEDULE_CHANGE_QUANTUM	|	\
		SCHEDULE_CHANGE_CPU		\
		)

#define schedule_process_local(p)	\
	schedule_process(p, SCHEDULE_CHANGE_PRIO | SCHEDULE_CHANGE_QUANTUM)
#define schedule_process_migrate(p)	\
	schedule_process(p, SCHEDULE_CHANGE_CPU)

#define CPU_DEAD	-1

#define cpu_is_available(c)	(cpu_proc[c] >= 0)

#define DEFAULT_USER_TIME_SLICE 200

/* processes created by RS are sysytem processes */
#define is_system_proc(p)	((p)->parent == RS_PROC_NR)

static unsigned cpu_proc[CONFIG_MAX_CPUS];

static void pick_cpu(struct schedproc * proc)
{
#ifdef CONFIG_SMP
	unsigned cpu, c;
	unsigned cpu_load = (unsigned) -1;

	if (machine.processors_count == 1) {
		proc->cpu = machine.bsp_id;
		return;
	}

	/* schedule sysytem processes only on the boot cpu */
	if (is_system_proc(proc)) {
		proc->cpu = machine.bsp_id;
		return;
	}

	/* if no other cpu available, try BSP */
	cpu = machine.bsp_id;
	for (c = 0; c < machine.processors_count; c++) {
		/* skip dead cpus */
		if (!cpu_is_available(c))
			continue;
		if (c != machine.bsp_id && cpu_load > cpu_proc[c]) {
			cpu_load = cpu_proc[c];
			cpu = c;
		}
	}
	proc->cpu = cpu;
	cpu_proc[cpu]++;
#else
	proc->cpu = 0;
#endif
}

/*Lottery Scheduling*/
/*Macro returns true if x is in user queue(i.e. between max user queue and min user queue) else false*/
#define PROCESS_IN_USER_Q(x) ((x)->priority >= MAX_USER_Q && \
				(x)->priority <= MIN_USER_Q)

/*===========================================================================*
 *				do_noquantum				     *
 *===========================================================================*/

int do_noquantum(message *m_ptr)
{
	register struct schedproc *rmp;
	int rv, proc_nr_n;

	if (sched_isokendpt(m_ptr->m_source, &proc_nr_n) != OK) {
		printf("SCHED: WARNING: got an invalid endpoint in OOQ msg %u.\n",
		m_ptr->m_source);
		return EBADEPT;
	}

	rmp = &schedproc[proc_nr_n];
	rmp->cputime += rmp->time_slice;
	printf("\nprocess id %u has cputime %ld\n", rmp->endpoint, rmp->cputime);

	/*Lottey Scheduling*/
	/*
        If the process is in the user queue then the priority remains unchanged and we are printing it's tickets number
        time slice and priority else if the process is not in the user queue then lower the priority by 1.
	*/
	if (PROCESS_IN_USER_Q(rmp)) {
		rmp->priority = USER_Q;
		printf("\nProcess %d is executed whose ticket no is %d with time quantum %d with priority %d\n", rmp->endpoint, rmp->ticketsNum, rmp->time_slice, rmp->priority);
	} else if (rmp->priority < MAX_USER_Q - 1) {
		rmp->priority += 1; /* lower priority */
	}

	if ((rv = schedule_process_local(rmp)) != OK) {
		return rv;
	}

	/*
        Lottey Scheduling
        After a process completed it's quantum do_lottery function is called to start another process
	*/
	if((rv = do_lottery()) != OK) {
		return rv;
	}

	return OK;
}

/*===========================================================================*
 *				do_stop_scheduling			     *
 *===========================================================================*/
int do_stop_scheduling(message *m_ptr)
{
	register struct schedproc *rmp;
	int proc_nr_n;
	int rv;

	/* check who can send you requests */
	if (!accept_message(m_ptr))
		return EPERM;

	if (sched_isokendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr_n) != OK) {
		printf("SCHED: WARNING: got an invalid endpoint in OOQ msg "
		"%ld\n", m_ptr->SCHEDULING_ENDPOINT);
		return EBADEPT;
	}

	rmp = &schedproc[proc_nr_n];
#ifdef CONFIG_SMP
	cpu_proc[rmp->cpu]--;
#endif
	rmp->flags = 0; /*&= ~IN_USE;*/

	/*
        Lottey Scheduling
        After a process has finished its execution do_lottery is called to start another process
    */
	if ((rv = do_lottery()) != OK) {
		return rv;
	}

	return OK;
}

/*===========================================================================*
 *				do_start_scheduling			     *
 *===========================================================================*/
int do_start_scheduling(message *m_ptr)
{
	register struct schedproc *rmp;
	int rv, proc_nr_n, parent_nr_n;

	/* we can handle two kinds of messages here */
	assert(m_ptr->m_type == SCHEDULING_START ||
		m_ptr->m_type == SCHEDULING_INHERIT);

	/* check who can send you requests */
	if (!accept_message(m_ptr))
		return EPERM;

	/* Resolve endpoint to proc slot. */
	if ((rv = sched_isemtyendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr_n))
			!= OK) {
		return rv;
	}
	rmp = &schedproc[proc_nr_n];

	/* Populate process slot */
	rmp->endpoint     = m_ptr->SCHEDULING_ENDPOINT;
	rmp->parent       = m_ptr->SCHEDULING_PARENT;
	rmp->max_priority = (unsigned) m_ptr->SCHEDULING_MAXPRIO;
	rmp->cputime = 0;

	/*Lottey Scheduling*/
	srandom();
	rmp->ticketsNum   = 5;

	if (rmp->max_priority >= NR_SCHED_QUEUES) {
		return EINVAL;
	}

	/* Inherit current priority and time slice from parent. Since there
	 * is currently only one scheduler scheduling the whole system, this
	 * value is local and we assert that the parent endpoint is valid */
	if (rmp->endpoint == rmp->parent) {
		/* We have a special case here for init, which is the first
		   process scheduled, and the parent of itself. */
		rmp->priority   = USER_Q;
		rmp->time_slice = DEFAULT_USER_TIME_SLICE;

		/*
		 * Since kernel never changes the cpu of a process, all are
		 * started on the BSP and the userspace scheduling hasn't
		 * changed that yet either, we can be sure that BSP is the
		 * processor where the processes run now.
		 */
#ifdef CONFIG_SMP
		rmp->cpu = machine.bsp_id;
		/* FIXME set the cpu mask */
#endif
	}

	switch (m_ptr->m_type) {

	case SCHEDULING_START:
		/* We have a special case here for system processes, for which
		 * quanum and priority are set explicitly rather than inherited
		 * from the parent */
		rmp->priority   = rmp->max_priority;
		rmp->time_slice = (unsigned) m_ptr->SCHEDULING_QUANTUM;
		break;

	case SCHEDULING_INHERIT:
		/* Inherit current priority and time slice from parent. Since there
		 * is currently only one scheduler scheduling the whole system, this
		 * value is local and we assert that the parent endpoint is valid */
		if ((rv = sched_isokendpt(m_ptr->SCHEDULING_PARENT,
				&parent_nr_n)) != OK)
			return rv;

		/*Lottery Scheduling*/
		//rmp->priority = schedproc[parent_nr_n].priority;
		rmp->priority = USER_Q; //default priority for any user process

		rmp->time_slice = schedproc[parent_nr_n].time_slice;
		break;

	default:
		/* not reachable */
		assert(0);
	}

	/* Take over scheduling the process. The kernel reply message populates
	 * the processes current priority and its time slice */
	if ((rv = sys_schedctl(0, rmp->endpoint, 0, 0, 0)) != OK) {
		printf("Sched: Error taking over scheduling for %d, kernel said %d\n",
			rmp->endpoint, rv);
		return rv;
	}
	rmp->flags = IN_USE;

	/* Schedule the process, giving it some quantum */
	pick_cpu(rmp);
	while ((rv = schedule_process(rmp, SCHEDULE_CHANGE_ALL)) == EBADCPU) {
		/* don't try this CPU ever again */
		cpu_proc[rmp->cpu] = CPU_DEAD;
		pick_cpu(rmp);
	}

	if (rv != OK) {
		printf("Sched: Error while scheduling process, kernel replied %d\n",
			rv);
		return rv;
	}

	/* Mark ourselves as the new scheduler.
	 * By default, processes are scheduled by the parents scheduler. In case
	 * this scheduler would want to delegate scheduling to another
	 * scheduler, it could do so and then write the endpoint of that
	 * scheduler into SCHEDULING_SCHEDULER
	 */

	m_ptr->SCHEDULING_SCHEDULER = SCHED_PROC_NR;

	return OK;
}

/*===========================================================================*
 *				do_nice					     *
 *===========================================================================*/
int do_nice(message *m_ptr)
{
	struct schedproc *rmp;
	int rv;
	int proc_nr_n;
	unsigned new_q, old_q, old_max_q;

	/*Lottery Scheduling*/
	int old_ticketsNum; //old ticketsNum value

	/* check who can send you requests */
	if (!accept_message(m_ptr))
		return EPERM;

	if (sched_isokendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr_n) != OK) {
		printf("SCHED: WARNING: got an invalid endpoint in OOQ msg "
		"%ld\n", m_ptr->SCHEDULING_ENDPOINT);
		return EBADEPT;
	}

	rmp = &schedproc[proc_nr_n];

    /*
        m_ptr->SCHEDULING_MAX_PRIO is set to the value of nice in servers/pm/schedule.c
    */
	new_q = (unsigned) m_ptr->SCHEDULING_MAXPRIO;

	rmp->ticketsNum += new_q;
	if(rmp->ticketsNum < 5)
		rmp->ticketsNum = 5;

    /*
        new_q is set the same value as the old_priority i.e. 13
    */
	new_q = rmp->priority;

	if (new_q >= NR_SCHED_QUEUES) {
		return EINVAL;
	}

	/* Store old values, in case we need to roll back the changes */
	old_q     = rmp->priority;
	old_max_q = rmp->max_priority;

	/*Lottery Scheduling*/
	old_ticketsNum = rmp->ticketsNum; //saving ticketsNum value to old_ticketsNum

	/* Update the proc entry and reschedule the process */

	/*Lottery Scheduling*/
	//rmp->max_priority = rmp->priority = new_q;
	rmp->priority = USER_Q; // default priority for user processes


	if ((rv = schedule_process_local(rmp)) != OK) {
		/* Something went wrong when rescheduling the process, roll
		 * back the changes to proc struct */
		rmp->priority     = old_q;
		rmp->max_priority = old_max_q;

		/*Lottery Scheduling*/
		rmp->ticketsNum = old_ticketsNum;//reverting ticketsNum value

	}

	/*Lottery Scheduling*/
	return do_lottery();//do_lottery is called to assign cpu to another process
}

/*===========================================================================*
 *				schedule_process			     *
 *===========================================================================*/
static int schedule_process(struct schedproc * rmp, unsigned flags)
{
	int err;
	int new_prio, new_quantum, new_cpu;

	pick_cpu(rmp);

	if (flags & SCHEDULE_CHANGE_PRIO)
		new_prio = rmp->priority;
	else
		new_prio = -1;

	if (flags & SCHEDULE_CHANGE_QUANTUM)
		new_quantum = rmp->time_slice;
	else
		new_quantum = -1;

	if (flags & SCHEDULE_CHANGE_CPU)
		new_cpu = rmp->cpu;
	else
		new_cpu = -1;

	if ((err = sys_schedule(rmp->endpoint, new_prio,
		new_quantum, new_cpu)) != OK) {
		printf("PM: An error occurred when trying to schedule %d: %d\n",
		rmp->endpoint, err);
	}

	return err;
}


/*===========================================================================*
 *				start_scheduling			     *
 *===========================================================================*/

void init_scheduling(void)
{
	/*Lottery Scheduling*/
	u64_t r;

	balance_timeout = BALANCE_TIMEOUT * sys_hz();
	init_timer(&sched_timer);
	set_timer(&sched_timer, balance_timeout, balance_queues, 0);

	/*Lottery Scheduling*/
	read_tsc_64(&r);
	srandom();

}

/*===========================================================================*
 *				balance_queues				     *
 *===========================================================================*/

/* This function in called every 100 ticks to rebalance the queues. The current
 * scheduler bumps processes down one priority when ever they run out of
 * quantum. This function will find all proccesses that have been bumped down,
 * and pulls them back up. This default policy will soon be changed.
 */
static void balance_queues(struct timer *tp)
{
	struct schedproc *rmp;
	int proc_nr;

	for (proc_nr=0, rmp=schedproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
		if (rmp->flags & IN_USE) {
			if (rmp->priority > rmp->max_priority && !PROCESS_IN_USER_Q(rmp)) {
				rmp->priority -= 1; /* increase priority */
				schedule_process_local(rmp);
			}
		}
	}

	set_timer(&sched_timer, balance_timeout, balance_queues, 0);
}

/*Lottery Scheduling*/
/*===========================================================================*
 *				do_lottery				     *
 *===========================================================================*/
int do_lottery() {
	struct schedproc *rmp;
	int proc_nr;
	int lucky;
	int old_priority;
	int flag = -1;
	int nTickets = 0;//Number of tickets hold by all processes in USER_Q

	/*calculates nTickets by adding tickets held by each process in USER_Q*/
	for (proc_nr=0, rmp=schedproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
		if ((rmp->flags & IN_USE) && PROCESS_IN_USER_Q(rmp)) {
			if(USER_Q == rmp->priority) {
				nTickets += rmp->ticketsNum;
			}
		}
	}

	//lucky no. is generated between 0 to nTickets
	lucky = nTickets ? random() % nTickets : 0;

	/*
        Selects process to schedule based  on lucky no.
        For each process in ready queue we use lucky = lucky - rmp->ticktsNum and if
        lucky < 0 we select that process to run by increasing the priority to 12.
	*/
	for (proc_nr=0, rmp=schedproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
		if ((rmp->flags & IN_USE) && PROCESS_IN_USER_Q(rmp) && USER_Q == rmp->priority) {//if process is in USER_Q
			old_priority = rmp->priority;												 //saving process priority to check whether process is elligible to run or not
			if (lucky >= 0) {															 //if lucky is greater than 0
				lucky -= rmp->ticketsNum;												 //subtract tickets held by the process from lucky
				if (lucky < 0) {														 //if lucky is less than 0
					rmp->priority = MAX_USER_Q;											 //change default priority of process to MAX_USER_Q
					flag = OK;															 //set flag to be OK
				}
			}
			if (old_priority != rmp->priority) {                                         //if priority of process changed
				schedule_process(rmp, flag);											 //schedule the process
			}
		}
	}
	return nTickets ? flag : OK;														 //return -1 if no process in USER_Q else OK
 }























