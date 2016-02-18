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
