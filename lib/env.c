/* See COPYRIGHT for copyright information. */

#include <mmu.h>
#include <error.h>
#include <env.h>
#include <pmap.h>
#include <printf.h>

struct Env *envs = NULL;		// All environments
struct Env *curenv = NULL;	        // the current env

static struct Env_list env_free_list;	// Free list

extern Pde * boot_pgdir;
//static int KERNEL_SP;
extern char * KERNEL_SP;
//
// Calculates the envid for env e.  
//


u_int mkenvid(struct Env *e)
{
	static u_long next_env_id = 0;
	// lower bits of envid hold e's position in the envs array
	u_int idx = e - envs;
	
	//printf("env.c:mkenvid:\txx:%x\n",(int)&idx);

	// high bits of envid hold an increasing number
	return(++next_env_id << (1 + LOG2NENV)) | idx;
}

//
// Converts an envid to an env pointer.
//
// RETURNS
//   env pointer -- on success and sets *error = 0
//   NULL -- on failure, and sets *error = the error number
//
int envid2env(u_int envid, struct Env **penv, int checkperm)
{
  
	struct Env *e;
	u_int cur_envid;
	if (envid == 0) {
		*penv = curenv;
		return 0;
	}

	e = &envs[ENVX(envid)];
	if (e->env_status == ENV_FREE || e->env_id != envid) {
		*penv = 0;
		return -E_BAD_ENV;
	}

	if (checkperm) {
		cur_envid = envid;
		while (&envs[ENVX(cur_envid)] != curenv && ENVX(cur_envid) != 0)
		{
			envid = envs[ENVX(cur_envid)].env_parent_id;
			cur_envid = envid;
		}
		if (&envs[ENVX(cur_envid)]!=curenv && ENVX(cur_envid) == 0)
		{
			*penv = 0;
			return -E_BAD_ENV;
		}
	}
	*penv = e;
	return 0;
	
}


//
// Marks all environments in 'envs' as free and inserts them into 
// the env_free_list.  Insert in reverse order, so that
// the first call to env_alloc() returns envs[0].
//
void
env_init(void)
{
	int i;
	
/*precondition: envs pointer has been initialized at mips_vm_init, called by mips_init*/
	/*1. initial env_free_list*/
	LIST_INIT(&env_free_list);
	//step 1;
	/*2. travel the elements in 'envs', initial every element(mainly initial its status, mark it as free) and inserts them into
	the env_free_list. attention :Insert in reverse order */
	for(i=NENV-1;i>=0;i--){
	      envs[i].env_status = ENV_FREE;
	      LIST_INSERT_HEAD(&env_free_list,envs+i,env_link);
	}

}

//
// Initializes the kernel virtual memory layout for environment e.
//
// Allocates a page directory and initializes it.  Sets
// e->env_cr3 and e->env_pgdir accordingly.
//
// RETURNS
//   0 -- on sucess
//   <0 -- otherwise 
//
static int
env_setup_vm(struct Env *e)
{
	// Hint:xtern char * KERNEL_SP;

	int i, r;
	struct Page *p = NULL;

	Pde *pgdir;
	if ((r = page_alloc(&p)) < 0)
	{
		panic("env_setup_vm - page_alloc error\n");
			return r;
	}
	p->pp_ref++;
	e->env_pgdir = (void *)page2kva(p);
	e->env_cr3 = page2pa(p);

	static_assert(UTOP % PDMAP == 0);
	for (i = PDX(UTOP); i <= PDX(~0); i++)
	  e->env_pgdir[i] = boot_pgdir[i];
	e->env_pgdir[PDX(VPT)]   = e->env_cr3 |PTE_V;
	e->env_pgdir[PDX(UVPT)]  = e->env_cr3 |PTE_V;

	return 0;
}

//
// Allocates and initializes a new env.
//
// RETURNS
//   0 -- on success, sets *new to point at the new env 
//   <0 -- on failure
//
int
env_alloc(struct Env **new, u_int parent_id)
{
	int r;
	/*precondtion: env_init has been called before this function*/
	/*1. get a new Env from env_free_list*/
	struct Env *currentE;
	currentE = LIST_FIRST(&env_free_list);
	/*2. call some function(has been implemented) to intial kernel memory layout for this new Env.
	 *hint:please read this c file carefully, this function mainly map the kernel address to this new Env address*/
	if((r=env_setup_vm(currentE))<0)
		return r;
	/*3. initial every field of new Env to appropriate value*/
	currentE->env_id = mkenvid(currentE);
	currentE->env_parent_id = parent_id;
	currentE->env_status = ENV_NOT_RUNNABLE;
	/*4. focus on initializing env_tf structure, located at this new Env. especially the sp register,
	 * CPU status and PC register(the value of PC can refer the comment of load_icode function)*/
	//currentE->env_tf.pc = 0x20+UTEXT;
	currentE->env_tf.regs[29] = USTACKTOP;
	currentE->env_tf.pc = UTEXT;
	currentE->env_tf.cp0_status = 0x10001004;
	currentE->env_runs = 0;
	//currentE->ipc_recving = 0;	
	/*5. remove the new Env from Env free list*/
	LIST_REMOVE(currentE,env_link);
	*new = currentE;
	//printf("%d",mkenvid(*new));
	return 0;
}

//
// Sets up the the initial stack and program binary for a user process.
//
// This function loads the complete binary image, including a.out header,
// into the environment's user memory starting at virtual address UTEXT,
// and maps one page for the program's initial stack at virtual address USTACKTOP - BY2PG.
// Since the a.out header from the binary is mapped at virtual address UTEXT,
// the actual program text starts at virtual address UTEXT+0x20.
//
static void
load_icode(struct Env *e, u_char *binary, u_int size)
{
	int r;
	u_long currentpg,endpg;

	currentpg = UTEXT;
//	printf("\ncurrentpg:%x\n",currentpg);
	endpg = currentpg + ROUND(size,BY2PG);
	//currentpg is since 0x0040 0000;so it is already rounded;
	/*precondition: we have a valid Env object pointer e, a valid binary pointer pointing to some valid 
	machine code(you can find them at $WORKSPACE/init/ directory, such as code_a_c, code_b_c,etc), which can
	 *be executed at MIPS, and its valid size */
//	binary+=0x1000;
	while(currentpg < endpg){
	     struct Page *page;
	     if((r= page_alloc(&page))<0)
		return;
	     if((r= page_insert(e->env_pgdir,page,currentpg,PTE_V|PTE_R))<0)
		return;
	     //printf("*binary:%8x\n",binary);
	     //printf("*page2kva:%8x\n",page2kva(page));
	     //bcopy((void *)binary,page2kva(page),BY2PG);
	     //bcopy((void *)binary,page2pa(page),BY2PG);
	     bzero(page2kva(page),BY2PG);
	     bcopy((void *)binary,page2kva(page),BY2PG);
	     //printf("copy succeed!\n");
	     binary += BY2PG;
	     currentpg +=BY2PG;
	}
	//currentpg = UTEXT;
	//bzero(currentpg,ROUND(size,BY2PG));
	//bcopy((void *)binary,(void *)currentpg,size);
	/*1. copy the binary code(machine code) to Env address space(start from UTEXT to high address), it may call some auxiliare function
	(eg,page_insert or bcopy.etc)*/
	struct Page *stack;
	page_alloc(&stack);
	page_insert(e->env_pgdir,stack,(USTACKTOP-BY2PG),PTE_V|PTE_R);
	//printf("Stack Set success\n");
	/*2. make sure PC(env_tf.pc) point to UTEXT + 0x20, this is very import, or your code is not executed correctly when your
//	 * process(namely Env) is dispatched by CPU*/
//	assert(e->env_tf.pc == UTEXT+0xb0);
	e->env_status = ENV_RUNNABLE;
	//printf("env_tf.pc:%x\n",e->env_tf.pc);
}

//
// Allocates a new env and loads the binary code into it.
//  - new env's parent env id is 0, please attention this
void
env_create(u_char *binary, int size)
{
	struct Env *e=NULL; 
	/*precondition: env_init has been called before, binary and size is valid, which can refer load_icode function*/
	/*Hint: this function wrap the env_alloc and load_icode function*/
	env_alloc(&e,0);
	//printf("alloc succeed!");
	/*1. allocate a new Env*/
	load_icode(e,binary+0x1000,size);

	/*2. load binary code to this new Env address space*/


}


//
// Frees env e and all memory it uses.
// 
void
env_free(struct Env *e)
{
	Pte *pt;
	u_int pdeno, pteno, pa;

	
	printf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

	// Flush all pages

	for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
		if (!(e->env_pgdir[pdeno] & PTE_V))
			continue;
		pa = PTE_ADDR(e->env_pgdir[pdeno]);
		pt = (Pte*)KADDR(pa);
		for (pteno = 0; pteno <= PTX(~0); pteno++)
			if (pt[pteno] & PTE_V)
				page_remove(e->env_pgdir, (pdeno << PDSHIFT) | (pteno << PGSHIFT));
		e->env_pgdir[pdeno] = 0;

		page_decref(pa2page(pa));
	}
	pa = e->env_cr3;
	e->env_pgdir = 0;
	e->env_cr3 = 0;
	page_decref(pa2page(pa));



	e->env_status = ENV_FREE;
	LIST_INSERT_HEAD(&env_free_list, e, env_link);
}

// Frees env e.  And schedules a new env
// if e is the current env.
//
void
env_destroy(struct Env *e) 
{
	env_free(e);
	if (curenv == e) {
		curenv = NULL;
		bcopy((int)KERNEL_SP-sizeof(struct Trapframe),TIMESTACK-sizeof(struct Trapframe),sizeof(struct Trapframe));
		printf("i am killed ... \n");
		sched_yield();
	}
}


extern void env_pop_tf(struct Trapframe *tf,int id);
extern void lcontext(u_int contxt);

/*please attention: this function called by real clock interrupt handler, like a callback function*/
/*parameter: struct Env* e, the process e will be the current env, and executed by CPU*/
/*function: this function complete the processes switch*/
void
env_run(struct Env *e)
{
	/*precondition: you have create at least one process(Env) in system.
	*/
	struct Trapframe *tf = NULL;
	if(curenv){
	      tf = TIMESTACK - sizeof(struct Trapframe);
	      bcopy(tf,&curenv->env_tf,sizeof(struct Trapframe));
	      curenv->env_tf.pc = tf->cp0_epc;
	}		
	/*1. keep the kernel path(namely trap frame) of the process which will be swapped out by CPU at TIMESTACK*/

	curenv = e;	
	/*2. set e to the current env*/
	e->env_runs ++;
	lcontext(KADDR(curenv->env_cr3));
	//printf("lcontext(*) success\n");
	/*3. keep current env's pgdir address in mCONTEXT, which will be used by tlb refill
	 * hint: please read the env_asm.S, you can find how to write it*/
//	printf("env_cr3:%8x\n",curenv->env_cr3);
//	printf("env_tf.pc:%8x\n",curenv->env_tf.pc);
	env_pop_tf(&(curenv->env_tf),GET_ENV_ASID(curenv->env_id));
	/*4. process switch, swap out old process, and run the new current env hint: please read the env_asm.S, you can find how to write it*/
//	printf("env_pop_tf() success\n");
}


