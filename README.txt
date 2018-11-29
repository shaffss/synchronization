Operating Systems Synchronization Project
* run ./<filename>
* set prefered mode:
	1) interactive OR
	2) file read OR
	3) /dev/urandom - pseudo random character generator
* run another terminal
	-send signals to any subprocess (display process tree with pids "pstree -p")
		1) SIGTSTP to pause
		2) SIGINT to continue
		3) SIGQUIT to end (kills all subprocesses and itself right after)
			//INFO - all processes work simultaneously informing each other by SIGUSR signal calls of their state
	-interactive mode works best with program cmds.
		these are:
			-pause
			-unpause
			-exit
				//they work pretty much the same as signal calls without need of opening additional terminal
enjoy
(c) Marcin Szafranski WAT student 2018