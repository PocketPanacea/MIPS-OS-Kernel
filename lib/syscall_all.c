#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>

extern char * KERNEL_SP;
extern struct Env * curenv;


void sys_putchar(int sysno,int c,int a2,int a3,int a4,int a5)
{
 	printcharc((char) c);
	return ;
}


void * memcpy(void *destaddr,void const *srcaddr,u_int len)
{
	char *dest = destaddr;
	char const *src = srcaddr;

	while(len-->0)
		*dest++=*src++;
	return destaddr;
}


// return the current environment id
u_int sys_getenvid(void)
{
	return curenv->env_id;
}


// deschedule current environment
void sys_yield(void)
{
	bcopy((int)KERNEL_SP-sizeof(struct Trapframe),TIMESTACK-sizeof(struct Trapframe),sizeof(struct Trapframe));
	sched_yield();
}

// destroy the current environment
void sys_env_destroy(int sysno,u_int envid)
{
/*
	printf("[%08x] exiting gracefully\n", curenv->env_id);
	env_destroy(curenv);
*/
	int r;
	struct Env *e;

	if ((r=envid2env(envid, &e, 1)) < 0)
		return r;
	printf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}


// Set envid's pagefault handler entry point and exception stack.
// (xstacktop points one byte past exception stack).
//
// Returns 0 on success, < 0 on error.
int sys_set_pgfault_handler(int sysno,u_int envid, u_int func, u_int xstacktop)
{
    ///////////////////////////////////////////////////////
    //your code here
	struct Env *e;
	int r = -1;
	if ( (r = envid2env(envid, &e, 0)) < 0){
		return r;
	}
	
	e->env_pgfault_handler = func;
	e->env_xstacktop = TRUP(xstacktop);
	return 0;

    //
    ///////////////////////////////////////////////////////
  	
}

//
// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
//
// If a page is already mapped at 'va', that page is unmapped as a
// side-effect.
//
// perm -- PTE_U|PTE_P are required, 
//         PTE_AVAIL|PTE_W are optional,
//         but no other bits are allowed (return -E_INVAL)
//
// Return 0 on success, < 0 on error
//	- va must be < UTOP
//	- env may modify its own address space or the address space of its children
// 
int sys_mem_alloc(int sysno,u_int envid, u_int va, u_int perm)
{
    ///////////////////////////////////////////////////////
    //your code here
	struct Env *e;
	struct Page *p;
	int ret = -1;

	if((perm & PTE_V) ==0)
	    panic("error");
	if (va >= UTOP)
		panic("error");
	
	if ((ret = envid2env(envid, &e, 1)) < 0)
		panic("envid2env");
	if ((ret = page_alloc(&p)) < 0)
		panic("envid2env");
	
	bzero(page2kva(p), BY2PG);
	
	if ((ret = page_insert(e->env_pgdir, p, va, perm)) < 0)
		panic("envid2env");
	
    return 0;		
    //
    ///////////////////////////////////////////////////////

}

// Map the page of memory at 'srcva' in srcid's address space
// at 'dstva' in dstid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_mem_alloc.
// (Probably we should add a restriction that you can't go from
// non-writable to writable?)
//
// Return 0 on success, < 0 on error.
//
// Cannot access pages above UTOP.
int sys_mem_map(int sysno,u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm)
{
    ///////////////////////////////////////////////////////
    //your code here
	struct Env *srcenv, *dstenv;
	struct Page *page;
	Pte *srcpte, *dstpte;

	
	if (envid2env(srcid, &srcenv, 0) < 0 || envid2env(dstid, &dstenv, 0) < 0)
		panic("error");
	
	srcva = ROUNDDOWN(srcva, BY2PG);
        dstva = ROUNDDOWN(dstva, BY2PG);

	if ((unsigned int)srcva >= UTOP || (unsigned int)dstva >= UTOP)
	    panic("error");
	if( (perm & PTE_V) ==0)
	    panic("error");
	if ((page = page_lookup(srcenv->env_pgdir, srcva, &srcpte)) == NULL)
		panic("error");
	if (page_insert(dstenv->env_pgdir, page, dstva, perm) != 0)
		panic("error");
	return 0;	
   ///////////////////////////////////////////////////////
	//	panic("sys_mem_map not implemented");
}

// Unmap the page of memory at 'va' in the address space of 'envid'
// (if no page is mapped, the function silently succeeds)
//
// Return 0 on success, < 0 on error.
//
// Cannot unmap pages above UTOP.
int sys_mem_unmap(int sysno,u_int envid, u_int va)
{
    ///////////////////////////////////////////////////////
    //your code here
	struct Env *task;
	if (envid2env(envid, &task, 1) < 0)
	{
		panic("UNMAP OF ENVID");
		panic("error");
	}
	if ((unsigned int)va >= UTOP || va != ROUNDDOWN(va, BY2PG))
	{
		panic("UNMAP OF VIRTUAL ADDRESS");
		panic("error");
	}
	page_remove(task->env_pgdir, va);
	
	return 0;	
	
    ///////////////////////////////////////////////////////
}

// Allocate a new environment.
//
// The new child is left as env_alloc created it, except that
// status is set to ENV_NOT_RUNNABLE and the register set is copied
// from the current environment.  In the child, the register set is
// tweaked so sys_env_alloc returns 0.
//
// Returns envid of new environment, or < 0 on error.
int sys_env_alloc(void)
{
    ///////////////////////////////////////////////////////
    //your code here
    //
	struct Env *child;

	if (env_alloc(&child, curenv->env_id) < 0)
		return -E_NO_FREE_ENV;
	bcopy(KERNEL_SP - sizeof(struct Trapframe), &child->env_tf, sizeof(struct Trapframe));
	
	child->env_status = ENV_NOT_RUNNABLE;
	child->env_pgfault_handler = curenv->env_pgfault_handler;
	child->env_tf.pc = child->env_tf.cp0_epc;	

	//tweak register exa of JOS(register v0 of MIPS) to 0 for 0-return
	child->env_tf.regs[2] = 0;
	
	return child->env_id;	

}

// Set envid's env_status to status. 
//
// Returns 0 on success, < 0 on error.
// 
// Return -E_INVAL if status is not a valid status for an environment.
int sys_set_env_status(int sysno,u_int envid, u_int status)
{
    ///////////////////////////////////////////////////////
    //your code here
	int r;
	struct Env *task;
	
	if ((r = envid2env(envid, &task, 0)) < 0){
		panic("sys_set_env_status failed");
		panic("error");
	}
	
	if (status != ENV_FREE && status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
	{
		panic("sys_set_env_status failed");
		panic("error");
	}
	task->env_status = status;
	return 0;


    ///////////////////////////////////////////////////////
	//panic("sys_env_set_status not implemented");
}

// Set envid's trap frame to tf.
//
// Returns 0 on success, < 0 on error.
//
// Return -E_INVAL if the environment cannot be manipulated.
int sys_set_trapframe(int sysno,u_int envid, struct Trapframe *tf)
{
	int r;
	struct Env *task;

	if ((r = envid2env(envid, &task, 1)) < 0)
		panic("error");

	task->env_tf = *tf;
	return 0;	

}

void sys_panic(int sysno,char *msg)
{
	// no page_fault_mode -- we are trying to panic!
	panic("%s", TRUP(msg));
}


// Block until a value is ready.  Record that you want to receive,
// mark yourself not runnable, and then give up the CPU.
void sys_ipc_recv(int sysno,u_int dstva)
{
    ///////////////////////////////////////////////////////
    //your code here
    //
	if ((unsigned)dstva >= UTOP || dstva != ROUNDDOWN(dstva, BY2PG)){
		return -E_INVAL ;
	}
	
	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_recving = 1;
	
	//curenv->env_tf.regs[2] = 0;
	//Mark Curenv ENV_NOT_RUNNABLE and Give Up CPU
	curenv->env_status = ENV_NOT_RUNNABLE;
	sys_yield();
    ///////////////////////////////////////////////////////
}

// Try to send 'value' to the target env 'envid'.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target has not requested IPC with sys_ipc_recv.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends
//    env_ipc_from is set to the sending envid
//    env_ipc_value is set to the 'value' parameter
// The target environment is marked runnable again.
//
// Return 0 on success, < 0 on error.
//
// Hint: the only function you need to call is envid2env.
int sys_ipc_can_send(int sysno,u_int envid, u_int value, u_int srcva, u_int perm)
{
    ///////////////////////////////////////////////////////
    //your code here
    //
	struct Env *target;
	struct Page *page;
	Pte *pte;
	int r, ret = 0;

	if ((r = envid2env(envid, &target, 0)) < 0)
		return -E_BAD_ENV;
	if (target->env_ipc_recving == 0)
		return -E_IPC_NOT_RECV;
	
	//srcva != NULL, MAP srcva
	if (srcva != 0 && target->env_ipc_dstva != 0 )
	{
		if ((unsigned int)srcva >= UTOP || srcva != ROUNDDOWN(srcva, BY2PG))
			panic("error");
		
		//page_lookup : return struct *Page virtual_address map to
		if ((page = page_lookup(curenv->env_pgdir, srcva, &pte)) == NULL)
			panic("error");
		
		if (page_insert(target->env_pgdir, page, target->env_ipc_dstva, perm) < 0)
			return -E_NO_MEM;
		
		ret = 1;
	}
	target->env_ipc_recving = 0;
	target->env_ipc_value = value;
	target->env_ipc_from = curenv->env_id;
	if (ret == 1)
		target->env_ipc_perm = perm;
	else
		target->env_ipc_perm = 0;
		
	
	target->env_status = ENV_RUNNABLE;

	return 0;
}

int sys_env_free(int sysno,u_int envid,u_int va,u_int size)
{
    printf("DEBUG:::sys_env_free start\n");
	Pte *pt;
	u_int pdeno, pteno, pa;
    struct Env *e;
    int r;

    if((r=envid2env(envid,&e,0))<0)
        panic("DEBUG::: sys_env_free envid not valid");
	
	printf("[%08x] start push new code\n", envid);


    int curpg=UTEXT;
    int endpg =curpg + ROUND(size-0x1000,BY2PG);

    while(curpg < endpg){
        struct Page *page;
        if((r=page_alloc(&page))<0)
            panic("DEBUG::::ERROR:::page_alloc");
        if((r=page_insert(e->env_pgdir,page,curpg,PTE_V|PTE_R|PTE_UC))<0)
            //UC is the flag of no clearing.
            panic("DEBUG::::ERROR:::page_inster");
        bzero(page2kva(page),BY2PG);
        bcopy((void *)va,page2kva(page),BY2PG);
        va += BY2PG;
        curpg +=BY2PG;
    }

    printf("[%08x] start deleteing old pages\n",envid);

	for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
		if (!(e->env_pgdir[pdeno] & PTE_V))
			continue;
		pa = PTE_ADDR(e->env_pgdir[pdeno]);
		pt = (Pte*)KADDR(pa);
		for (pteno = 0; pteno <= PTX(~0); pteno++)
			if ((pt[pteno] & PTE_V)&&(!pt[pteno] & PTE_UC))
				page_remove(e->env_pgdir, (pdeno << PDSHIFT) | (pteno << PGSHIFT));
		//e->env_pgdir[pdeno] = 0;
		//page_decref(pa2page(pa));
	}

    struct Page *stack;
    page_alloc(&stack);
    page_insert(e->env_pgdir,stack,(USTACKTOP-BY2PG),PTE_V|PTE_R);
    e->env_tf.cp0_epc = UTEXT;
    //e->env_tf.pc = UTEXT;
    e->env_tf.regs[29] = USTACKTOP;

	bcopy(&(e->env_tf),(void *)KERNEL_SP-sizeof(struct Trapframe),sizeof(struct Trapframe));

}
