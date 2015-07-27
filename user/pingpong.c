// Ping-pong a counter between two processes.
// Only need to start one of these -- splits into two with fork.

#include "lib.h"

#define OFFSET(e) (u_int)&(((struct Env*)0)->e)
#define DEBUG 1

void
umain(void)
{
	struct Env *chenv;
	int ch;

	if (DEBUG)
	{
		writef("--------------DEBUG-------------\n");
		writef("print Env env_tf: %d\n",OFFSET(env_tf));
		writef("print Env env_link: %d\n",OFFSET(env_link));
		writef("print Env env_id: %d\n",OFFSET(env_id));
		writef("print Env env_parent_id: %d\n",OFFSET(env_parent_id));
		writef("print Env env_pgdir: %d\n",OFFSET(env_pgdir));
		writef("print Env env_cr3: %d\n",OFFSET(env_cr3));
		writef("print Env env_prio: %d\n",OFFSET(env_prio));
		writef("print Env temp: %d\n",OFFSET(temp));
		writef("print Env env_ipc_value: %d\n",OFFSET(env_ipc_value));
		writef("print Env env_runs: %d\n",OFFSET(env_runs));
		writef("print Env Sizeof: %d\n",sizeof(struct Env));
		writef("--------------DEBUG-------------\n");
	}

	syscall_set_env_prio(0, PRIO_HIGHEST);

	if ((ch = fork()) != 0)
	{
//		writef("DEBUG: ch value: %d\n", ch);
		if (syscall_set_env_prio(ch, PRIO_LOW) < 0)
			user_panic("Not Found Env");
	
		if((ch = fork()) != 0)
		{
			if (syscall_set_env_prio(ch, PRIO_HIGH) < 0)
				user_panic("Not Found Env");
			
			if ((ch = fork()) != 0)
			{
				if (syscall_set_env_prio(ch, PRIO_LOWEST) < 0)
					user_panic("Not Found Env");
			}
		}	
	}
	
	chenv = (struct Env*)envs + ENVX(syscall_getenvid());
	
	if (chenv->env_prio == PRIO_HIGHEST)
		writef("highest\n");
	if (chenv->env_prio == PRIO_HIGH)
		writef("high\n");
	if (chenv->env_prio == PRIO_NORM)
		writef("normal\n");
	if (chenv->env_prio == PRIO_LOW)
		writef("low\n");
	if (chenv->env_prio == PRIO_LOWEST)
		writef("lowest\n");
	
}

