// User-level IPC library routines

#include "lib.h"#include <mmu.h>
#include <env.h>

extern struct Env *env;

// Send val to whom.  This function keeps trying until
// it succeeds.  It should panic() on any error other than
// -E_IPC_NOT_RECV.  
//
// Hint: use syscall_yield() to be CPU-friendly.
void
ipc_send(u_int whom, u_int val, u_int srcva, u_int perm)
{
	int r;

	while ((r=syscall_ipc_can_send(whom, val, srcva, perm)) == -E_IPC_NOT_RECV)
	{
		syscall_yield();
		//writef("QQ");
	}
	if(r == 0)
		return;
	user_panic("error in ipc_send: %d", r);
}

// Receive a value.  Return the value and store the caller's envid
// in *whom.  
//
// Hint: use env to discover the value and who sent it.
u_int
ipc_recv(u_int *whom, u_int dstva, u_int *perm)
{
//printf("ipc_recv:come 0\n");
	syscall_ipc_recv(dstva);
	env->env_ipc_recving = 1;
	while( env->env_ipc_recving == 1 )writef("");
	
	if (whom)
		*whom = env->env_ipc_from;
	if (perm)
		*perm = env->env_ipc_perm;
	return env->env_ipc_value;
}

void
new_ipc_send(u_int whom, u_int val, u_int srcva, u_int perm){
    
	u_int who, ack;
	int r;

	//send val to whom and get the ack
	who = syscall_getenvid();

	ipc_send(whom, val, srcva, perm);
	ack = ipc_recv(&who, 0, 0);

	writef("%x got ack %d from %x\n", syscall_getenvid(), ack, who);
	if ((ack - val) == 1)
	{
		//send ack - 1 to whom
		ipc_send(whom, ack - 1, srcva, perm);
	}
	else
		user_panic("transmit failed in new_ipc_send!\n");
}

u_int
new_ipc_recv(u_int *whom, u_int srcva, u_int *perm){

	u_int who, recv, ack;
	//1.get the val 2.send the ack(val+1) back 3.get the ack
	who = *whom;

	recv = ipc_recv(whom, srcva, perm);
	ipc_send(*whom, recv + 1, 0, 0);
	ack = ipc_recv(&who, srcva, perm);

	if (ack == recv)
	{
		return recv;
	}
	else
	{
		user_panic("transmit failed in new_ipc_recv!\n");
	}

}



