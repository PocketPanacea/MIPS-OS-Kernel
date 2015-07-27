#include <env.h>
#include <pmap.h>
#include <printf.h>

// Trivial temporary clock interrupt handler,
// called from clock_interrupt in locore.S
/*void
clock(void)
{
	printf("*");
}*/


// The real clock interrupt handler,
// implementing round-robin scheduling
void
sched_yield_debug(void)
//idle env's mode
{

	/*precondition: please make sure that there are at least one process(env) has been created and created in
	 * the system*/
	static int nextEnv = 0;
	static int printEnv = -1;

	int i = 0;
	int temp = -1;

	for (i = 0; i < NENV; i++){
		if (envs[nextEnv].env_status == ENV_RUNNABLE)
		{
			temp = nextEnv;
			nextEnv = ( nextEnv + 1 ) % NENV;
			if (temp != 0)
			{
				env_run(&envs[temp]);
			}
		}
		else
		{
			nextEnv = ( nextEnv + 1 ) % NENV;
		}
	}

	/*call the function we have implemented at env.c file, in which file I have give you some hints*/
	env_run(&envs[0]);
	panic("No Env to run!");
}

void
sched_yield(void)
{
	static int nextEnv = 0, printEnv;

	while(envs[nextEnv].env_status != ENV_RUNNABLE)
	{
        	nextEnv = (nextEnv + 1) % NENV;
	}
	
	printEnv = nextEnv % NENV;
	nextEnv += 1;
	
	env_run(&envs[printEnv]);
}

