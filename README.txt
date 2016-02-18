Description:

The purpose of this project is to modify the existing Minix scheduler to implement and analyze Multilevel Feedback Queue Scheduling and Lottery Scheduling. The performance analysis is done based on the Turnaround time(TAT) and Waiting Time(WT) for each process.

How to Run:

The Source Code Folder contains the necessary source code of the files to be copied into the Minix OS.

Multilevel Feedback Queue Scheduler
	Copy the files in the respective folders
	Execute the following commands
		# cd /usr/src/
		# make world
		# reboot
	To check the scheduler, run mytest
		# cc longrun1.c -o long1
		# cc longrun2.c -o long2
		# cc longrun3.c -o long3
		# cc mytest.c -o mytest
	
Lottery Scheduler
	Copy the files in the respective folders
	pm_schedule.c is to be copied into the /usr/src/servers/pm/ folder with name schedule.c
	sched_schedule.c is to be copied into the /usr/src/servers/sched/ folder with name schedule.c
	Execute the following commands
		# cd /usr/src/releasetools/
		# make clean install
		# reboot
	To check the scheduler, run mytest
		# cc longrun1.c -o long1
		# cc longrun2.c -o long2
		# cc longrun3.c -o long3
		# cc mytest.c -o mytest

Refer design pdf for more information. The long1, long2, long3 in the above description refers to the processes used to check the scheduler.
