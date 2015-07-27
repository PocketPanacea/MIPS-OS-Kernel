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
sched_yield_JOS(void)
//idle env' mode
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
sched_yield_origin(void)
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


void
sched_yield(void)
{
	int j = 0;
	int base_env = 0, run_env = 0;
	int max_priority = 0;
	struct Env *current = (curenv == NULL || curenv >= envs + NENV - 1) ? envs + 1 : curenv + 1;
	
	for (j = 0, base_env = current - envs, max_priority = 0; j < NENV; j++)
	{
		if (0 != ((base_env + j) % NENV) &&
		    envs[(base_env + j) % NENV].env_status == ENV_RUNNABLE &&
		    envs[(base_env + j) % NENV].env_prio > max_priority
		   )
		{
			max_priority = envs[(base_env + j) % NENV].env_prio;
			run_env = (base_env + j) % NENV;
		}
	}
	if (max_priority != 0)
	{
		env_run(&envs[run_env]);
		return;
	}		

	if (envs[0].env_status == ENV_RUNNABLE)
		env_run(&envs[0]);
	else
		panic("No Env to Run\n");
}

