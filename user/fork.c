// implement fork from user space

#include "lib.h"
#include <mmu.h>
#include <env.h>

//fmars define a free mem space for pgfault
#define PFTEMP (ROUNDDOWN(0x50000000, BY2PG))//( USTACKTOP - 2 * BY2PG )
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//

void user_bcopy(const void *src, void *dst, size_t len)
{
	void *max;

	//	writef("~~~~~~~~~~~~~~~~ src:%x dst:%x len:%x\n",(int)src,(int)dst,len);
	max = dst + len;
	// copy machine words while possible
	if (((int)src%4==0)&&((int)dst%4==0))
	{
		while (dst + 3 < max)
		{
			*(int *)dst = *(int *)src;
			dst+=4;
			src+=4;
		}
	}
	
	// finish remaining 0-3 bytes
	while (dst < max)
	{
		*(char *)dst = *(char *)src;
		dst+=1;
		src+=1;
	}
	//for(;;);
}


void user_bzero(void *v, u_int n)
{
	char *p;
	int m;

	p = v;
	m = n;
	while (--m >= 0)
		*p++ = 0;
}


//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//

static void
pgfault(u_int va)
{
	int r;
	int i;
	
	va = ROUNDDOWN(va, BY2PG);
//	writef("-----------------------------\nthe address is %x\n--------------------\n",va);	

	if (!((*vpt)[VPN(va)] & PTE_COW ))
		user_panic("syscall_mem_alloc failed!");
	if (syscall_mem_alloc(0, PFTEMP, PTE_R) < 0)
		user_panic("syscall_mem_alloc failed!");
	
	user_bcopy((void*)va, PFTEMP, BY2PG);
	
	if (syscall_mem_map(0, PFTEMP, 0, va, PTE_R) < 0)
		user_panic("syscall_mem_map failed!");
	if (syscall_mem_unmap(0, PFTEMP) < 0)
		user_panic("syscall_mem_unmap failed!");

}

//
// Map our virtual page pn (address pn*BY2PG) into the target envid
// at the same virtual address.  if the page is writable or copy-on-write,
// the new mapping must be created copy on write and then our mapping must be
// marked copy on write as well.  (Exercise: why mark ours copy-on-write again if
// it was already copy-on-write?)
// 

/*Fmars Try the asm funciont about *p
void t(){
	int *p,*q;
	*p = 1234;
	
}*/
static void
duppage(u_int envid, u_int pn)
{
	int r;
	u_int addr;
	Pte pte;
	u_int perm;
	
	perm = ((*vpt)[pn]) & 0xfff;
	if( (perm & PTE_R)!= 0 || (perm & PTE_COW)!= 0)
	{
		writef("");
		perm = PTE_V|PTE_COW;
		if(syscall_mem_map(0, pn * BY2PG, envid, pn * BY2PG, perm) == -1)
			user_panic("duppage failed at 1");

		if(syscall_mem_map(0, pn * BY2PG, 0, pn * BY2PG, perm) == -1)
                        user_panic("duppage failed at 2");
	}
	else{
		if(syscall_mem_map(0, pn * BY2PG,envid, pn * BY2PG, perm) == -1)
			user_panic("duppage failed at 3");
	}
}


//
// User-level fork.  Create a child and then copy our address space
// and page fault handler setup to the child.
//
// Hint: use vpd, vpt, and duppage.
// Hint: remember to fix "env" in the child process!
// 
extern void __asm_pgfault_handler(void);
int
fork(void)
{
	// Your code here.
	u_int envid;
	int pn;
	extern struct Env *envs;
	extern struct Env *env;

	set_pgfault_handler(pgfault);

	if((envid = syscall_env_alloc()) < 0)
		user_panic("syscall_env_alloc failed!");
	
	if(envid == 0)
	{
		env = &envs[ENVX(syscall_getenvid())];
		return 0;
	}
	

	
	for(pn = 0; pn < ( UTOP / BY2PG) - 1 ; pn ++){	
		if(((*vpd)[pn/PTE2PT]) != 0 && ((*vpt)[pn]) != 0){
			duppage(envid, pn);
		}
	}
	
	if(syscall_mem_alloc(envid, UXSTACKTOP - BY2PG, PTE_V|PTE_R) < 0)
		user_panic("syscall_mem_alloc failed!");
	if(syscall_set_pgfault_handler(envid, __asm_pgfault_handler, UXSTACKTOP) < 0)
		user_panic("syscall_set_pgfault_handler failed!");
	if(syscall_set_env_status(envid, ENV_RUNNABLE) < 0)
		user_panic("syscall_set_env_status failed");
//	writef("DEBUG: fork: envid: %d\n", envid);
	return envid;	
}

// Challenge!
int
sfork(void)
{
	user_panic("sfork not implemented");
	return -E_INVAL;
}
