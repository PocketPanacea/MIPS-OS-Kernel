#include "mmu.h"
#include "pmap.h"
#include "printf.h"
#include "env.h"
#include "error.h"



/* These variables are set by mips_detect_memory() */
u_long maxpa;            /* Maximum physical address */
u_long npage;            /* Amount of memory(in pages) */
u_long basemem;          /* Amount of base memory(in bytes) */
u_long extmem;           /* Amount of extended memory(in bytes) */

Pde* boot_pgdir;

struct Page *pages;
static u_long freemem;

static struct Page_list page_free_list;	/* Free list of physical pages */

void mips_detect_memory()
{
	
	basemem = 64*1024*1024;
	extmem = 0;
	maxpa = basemem;

	npage = maxpa / BY2PG;

	printf("Physical memory: %dK available, ", (int)(maxpa/1024));
	printf("base = %dK, extended = %dK\n", (int)(basemem/1024), (int)(extmem/1024));
}

static void * alloc(u_int n, u_int align, int clear)
{
   //end is defined in mips_vm_init()
	extern char end[]; 	
	u_long alloced_mem;
	// initialize freemem if this is the first time
	if (freemem == 0){
		freemem = (u_long)end;
	}

	// Your code here:
	//	Step 1: round freemem up to be aligned properly
	freemem = ROUND(freemem, align);
	//#define ROUND(a, n)(((((u_long)(a))+(n)-1))&~((n)-1)) caculate with ~(align-1) for aligned (align = pow(2,int))

	//	Step 2: save current value of freemem as allocated chunk
	alloced_mem = freemem;
	//	Step 3: increase freemem to record allocation
	freemem += n;
	//	Step 4: clear allocated chunk if necessary

	if (PADDR(freemem) >= maxpa){
		panic("Memory Shortage\n");
		return (void*)(-E_NO_MEM);
	}

	if (clear){
		bzero((void*)alloced_mem, n);
	}
	//	Step 5: return allocated chunk
	//how to return allocated chunk?? 
	//save the allocated address to an globe variable
	return (void *)alloced_mem;

}


//
// Given pgdir, the current page directory base,
// walk the 2-level page table structure to find
// the Pte for virtual address va.  Return a pointer to it.
//
// If there is no such directory page:
//	- if create == 0, return 0.
//	- otherwise allocate a new directory page and install it.
//
// This function is abstracting away the 2-level nature of
// the page directory for us by allocating new page tables
// as needed.
// 
// Boot_pgdir_walk cannot fail.  It's too early to fail.
// 
static Pte* boot_pgdir_walk(Pde *pgdir, u_long va, int create)
{
	Pde *targetPde;
	Pte *pageTable;

	targetPde = (Pde *)(&pgdir[PDX(va)]);
	pageTable = (Pte *)KADDR(PTE_ADDR(*targetPde));
	
	if( *targetPde == 0){
		if(create == 0)
			return 0;
		else{
		//create a new directory page,or allocating new page tables
		//the page size equal to a page table struct size 4096 = 4 * 1024 = 4 * 2^10
			pageTable = alloc(BY2PG, BY2PG, 1);


		//what is the function of PTE_R
		//the entries of page directory and page table store physical address
			*targetPde = PADDR(pageTable) | PTE_R | PTE_V;
		
		}
	}
	
	return (Pte *)(&pageTable[PTX(va)]);

}
//
// Map [va, va+size) of virtual address space to physical [pa, pa+size)
// in the page table rooted at pgdir.  Size is a multiple of BY2PG.
// Use permission bits perm|PTE_V for the entries.
//
void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)
{
	//v?????????????????
	int nToMap,i;
	Pte * pageTable;

	nToMap = size / BY2PG;

	for (i = 0; i < nToMap; i++){
		pageTable = boot_pgdir_walk(pgdir, va, 1);
		*pageTable = pa | perm | PTE_V;
		
		pa += BY2PG;
		va += BY2PG;
	}
}

// Set up a two-level page table:
// 
// This function only sets up the kernel part of the address space
// (ie. addresses >= UTOP).  The user part of the address space
// will be setup later.
//
void mips_vm_init()
{
	extern char KVPT[];
	extern char end[];
	extern int mCONTEXT;
	extern struct Env *envs;
	struct Env *en;

	Pde* pgdir;
	u_int n;
	

	//v?????????????????
	pgdir = (Pde *)alloc(BY2PG,BY2PG,1);
	printf("to memory %x for struct page directory.\n",freemem);
	//mCONTEXT = (int)(*pgdir);
	mCONTEXT = (int)(pgdir);
	boot_pgdir = (Pde *)mCONTEXT;

	pages = (struct Page*)alloc(sizeof(struct Page)*npage,BY2PG,1);
	printf("to memory %x for struct Pages.\n",freemem);
	//create pgdir entry for all kernel virtual memory

	boot_pgdir_walk(pgdir,ULIM,1);
	boot_map_segment(pgdir,ULIM,PDMAP,0,0);
	//before this print information cannot be printed
	printf("mips_vm_init:boot_pgdir is %x\n",boot_pgdir);

	printf("pmap.c:\t mips vm init success\n");
	envs = (struct Env*)alloc(NENV*sizeof(struct Env),BY2PG,1);
	boot_map_segment(pgdir,UENVS,NENV*sizeof(struct Env),PADDR(envs),PTE_R);
	
}





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------
// Tracking of physical pages.
// -
//static void page_initpp(struct Page *pp);
void
page_init(void)
{
	// The exaple code here marks all pages as free.
	// However this is not truly the case.  What memory is free?
	//  1) Mark page 0 as in use(for good luck) 
	//  2) Mark the rest of base memory as free.
	//  3) Then comes the IO hole [IOPHYSMEM, EXTPHYSMEM) => mark it as in use
	//     So that it can never be allocated.      
	//  4) Then extended memory(ie. >= EXTPHYSMEM):
	//     ==> some of it's in use some is free. Where is the kernel?
	//     Which pages are used for page tables and other data structures?    
	//
	// Change the code to reflect this.
	//v?????????????????
	int i;
	LIST_INIT (&page_free_list);
	freemem = ROUND(freemem, BY2PG);

	for (i = 0; i < PADDR(freemem) / BY2PG; i++){
		pages[i].pp_ref = 1;
	}

	for (; i < npage; i++){
		pages[i].pp_ref = 0;
		LIST_INSERT_HEAD(&page_free_list, &pages[i], pp_link);
	}
	
}

//
// Initialize a Page structure.
//
static void
page_initpp(struct Page *pp)
{
	bzero(pp, sizeof(*pp));
}


//
// Allocates a physical page.
//
// *pp -- is set to point to the Page struct of the newly allocated
// page
//
// RETURNS 
//   0 -- on success
//   -E_NO_MEM -- otherwise 
//
// Hint: use LIST_FIRST, LIST_REMOVE, page_initpp()
// Hint: pp_ref should not be incremented 
int
page_alloc(struct Page **pp)
{
	// Fill this function in
	struct Page *page_temp;
	page_temp = LIST_FIRST(&page_free_list);

	//pick the head of the free list
	//then remove the first(head) element of the page free list :page_free_list
	if(page_temp != 0){
		//see page_check to see why use **pp here
		*pp = page_temp;	//set NULL
		LIST_REMOVE(page_temp, pp_link);

		//(*pp)->pp_ref++;
		//printf("%x\n",*pp);
	

		//page initialization
		page_initpp(*pp);	//refresh page
		//set(*pp)-pages to get the number of pages << 12
		bzero(KADDR(page2pa(*pp)), BY2PG);	//refresh page->4K space
		
		
		return 0;
	}
	
	//if know the address of the allocated Page ,pp pointed to .
	//then we can get the index of pp in Page arrays:pages.
	//then we can caculate the real physical address range of pp.
	
		
	
	return -E_NO_MEM;
}


//
// Return a page to the free list.
// (This function should only be called when pp->pp_ref reaches 0.)
//
void
page_free(struct Page *pp)
{
	// Fill this function in
	//when to free a page ,just insert it to the page_free_list.
	if (pp->pp_ref > 0){
		return ;
	}
	else if (pp->pp_ref == 0){
		LIST_INSERT_HEAD(&page_free_list, pp, pp_link);
		return ;
	}
	else{
		LIST_INSERT_HEAD(&page_free_list, pp, pp_link);
		return ;
		//Invalid pp_ref Operation
	}
	
	//LIST_INSERT_HEAD(&page_free_list,pp,pp_link);
	
}



void
page_decref(struct Page *pp)
{
	if (--pp->pp_ref == 0)
		page_free(pp);
}


//
// This is boot_pgdir_walk with a different allocate function.
// Unlike boot_pgdir_walk, pgdir_walk can fail, so we have to
// return pte via a pointer parameter.
//
// Stores address of page table entry in *pte.
// Stores 0 if there is no such entry or on error.
// 
// RETURNS: 
//   0 on success
//   -E_NO_MEM, if page table couldn't be allocated
//


int
pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)
{
	// Fill this function in
	//v?????????????????
	Pde *targetPde;
	Pte *pageTable;
	
	struct Page *pageTablePage;
	targetPde = (Pde *)(&pgdir[PDX(va)]);
	pageTable = (Pte *)KADDR(PTE_ADDR(*targetPde));
	
	if ((*targetPde & PTE_V) == 0){
		if (create == 0){
			*ppte = 0;
			return 0;
		}
		else
		{
			if (page_alloc(&pageTablePage) != 0){
				*ppte = 0;
				return -E_NO_MEM;
			}
		
			pageTable = KADDR(page2pa(pageTablePage));
			*targetPde = PADDR(pageTable) | PTE_R | PTE_V;
			pageTablePage->pp_ref++;
		}

		//printf("page allocate succeed in pgdir_walk");

		//init the newly allocated physical page,used as Page Table. 

	}	
	
	//printf("pgdir_walk succeed.allocated page is %x\n",pageTable);
	*ppte = (Pte*)(&pageTable[PTX(va)]);	//equals -> va->PDE
	
        return 0;
}



void
tlb_invalidate(Pde *pgdir, u_long va)
{
	if (curenv)
		tlb_out(PTE_ADDR(va)|GET_ENV_ASID(curenv->env_id));
	else
		tlb_out(PTE_ADDR(va));

}


//
// Map the physical page 'pp' at virtual address 'va'.
// The permissions (the low 12 bits) of the page table
//  entry should be set to 'perm|PTE_P'.
//
// Details
//   - If there is already a page mapped at 'va', it is page_remove()d.
//   - If necesary, on demand, allocates a page table and inserts it into 'pgdir'.
//   - pp->pp_ref should be incremented if the insertion succeeds
//
// RETURNS: 
//   0 on success
//   -E_NO_MEM, if page table couldn't be allocated
//
// Hint: The TA solution is implemented using
//   pgdir_walk() and and page_remove().
//
/*int
page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm) 
{
	// Fill this function in
	//v?????????????????
	Pte *ptEntry;
	struct Page *oldPage;
	if(pgdir_walk(pgdir,va,1,&ptEntry) != 0){
		return -E_NO_MEM;
	}
	if(*ptEntry != 0){
		oldPage = pa2page(*ptEntry);
		page_decref(oldPage);
	}
	*ptEntry = page2pa(pp)|perm|PTE_V;
	pp->pp_ref++;
	//A?????????????????
	printf("%x page_insert succeed!\n",pp);
	return 0;
}*/

int
page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm) 
{	// Fill this function in
	u_int PERM;	
	Pte *pgtable_entry;
	PERM = perm | PTE_V;
	
        pgdir_walk(pgdir, va, 0, &pgtable_entry);

	if(pgtable_entry!=0 &&(*pgtable_entry & PTE_V)!=0)
		if(pa2page(*pgtable_entry)!=pp) page_remove(pgdir,va);
		else
		{
			tlb_invalidate(pgdir, va);
			*pgtable_entry = (page2pa(pp)|PERM);			
			return 0;
		}
        tlb_invalidate(pgdir, va);
	if(pgdir_walk(pgdir, va, 1, &pgtable_entry)!=0)
		return -E_NO_MEM;// panic("page insert wrong.\n");
	*pgtable_entry = (page2pa(pp)|PERM);
	pp->pp_ref++;
	return 0;
}



//return the Page which va map to.
struct Page*
page_lookup(Pde *pgdir, u_long va, Pte **ppte)
{
	struct Page *ppage;
	Pte *pte;

        pgdir_walk(pgdir, va, 0, &pte);
       	if(pte==0) return 0;
       	if((*pte & PTE_V)==0) return 0;       //the page is not in memory.
       	ppage = pa2page(*pte);
	if(ppte) *ppte = pte;
	return ppage;
}


//
// Unmaps the physical page at virtual address 'va'.
//
// Details:
//   - The ref count on the physical page should decrement.
//   - The physical page should be freed if the refcount reaches 0.
//   - The pg table entry corresponding to 'va' should be set to 0.
//     (if such a PTE exists)
//   - The TLB must be invalidated if you remove an entry from
//	   the pg dir/pg table.
//
// Hint: The TA solution is implemented using
//  pgdir_walk(), page_free(), tlb_invalidate()
//

void
page_remove(Pde *pgdir, u_long va) 
{
	// Fill this function in
	//v?????????????????
	Pte * ppte;
	u_long pa;
	
	pa = va2pa(pgdir,va);
	//PTX is error,should use PPN(pa),PTX just have 12 bit valid number
	//#define PTX(va)		((((u_long)(va))>>12) & 0x03FF)
	//#define PPN(va)		(((u_long)(va))>>12)
	//page_decref(&pages[PTX(pa)]);
	page_decref(pa2page(pa));

	pgdir_walk(pgdir,va,0, &ppte);
	*ppte = 0;
	tlb_invalidate(pgdir,va);

	
	return;
}



void
page_check(void)
{
	struct Page *pp, *pp0, *pp1, *pp2;
	struct Page_list fl;

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert(page_alloc(&pp0) == 0);
	assert(page_alloc(&pp1) == 0);
	assert(page_alloc(&pp2) == 0);

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	//after this this page_free list must be empty!!!!
	LIST_INIT(&page_free_list);
	

	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);
	printf("start page_insert\n");
	// there is no free memory, so we can't allocate a page table 
	assert(page_insert(boot_pgdir, pp1, 0x0, 0) < 0);

	// free pp0 and try again: pp0 should be used for page table
	page_free(pp0);
	//va address 0x0 pageout error "too low"
 	assert(page_insert(boot_pgdir, pp1, 0x0, 0) == 0);
 	//assert(page_insert(boot_pgdir, pp1, 0x10000, 0) == 0);
	assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));
	printf("va2pa(boot_pgdir, 0x0) is %x\n",va2pa(boot_pgdir, 0x0));
	printf("page2pa(pp1) is %x\n",page2pa(pp1));
	assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
	assert(pp1->pp_ref == 1);

	// should be able to map pp2 at BY2PG because pp0 is already allocated for page table
	assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
	assert(pp2->pp_ref == 1);
	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);
//printf("why\n");
	// should be able to map pp2 at BY2PG because it's already there
	assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
	assert(pp2->pp_ref == 1);
//printf("It is so unbelivable\n");
	// pp2 should NOT be on the free list
	// could happen in ref counts are handled sloppily in page_insert
	assert(page_alloc(&pp) == -E_NO_MEM);

	// should not be able to map at PDMAP because need free page for page table
	assert(page_insert(boot_pgdir, pp0, PDMAP, 0) < 0);

	// insert pp1 at BY2PG (replacing pp2)
	assert(page_insert(boot_pgdir, pp1, BY2PG, 0) == 0);

	// should have pp1 at both 0 and BY2PG, pp2 nowhere, ...
	assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
	// ... and ref counts should reflect this
	assert(pp1->pp_ref == 2);
	printf("pp2->pp_ref %d\n",pp2->pp_ref);
	assert(pp2->pp_ref == 0);
	printf("end page_insert\n");

	// pp2 should be returned by page_alloc
	assert(page_alloc(&pp) == 0 && pp == pp2);

	// unmapping pp1 at 0 should keep pp1 at BY2PG
	page_remove(boot_pgdir, 0x0);
	assert(va2pa(boot_pgdir, 0x0) == ~0);
	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp2->pp_ref == 0);

	// unmapping pp1 at BY2PG should free it
	page_remove(boot_pgdir, BY2PG);
	assert(va2pa(boot_pgdir, 0x0) == ~0);
	assert(va2pa(boot_pgdir, BY2PG) == ~0);
	assert(pp1->pp_ref == 0);
	assert(pp2->pp_ref == 0);

	// so it should be returned by page_alloc
	assert(page_alloc(&pp) == 0 && pp == pp1);

	// should be no free memory
	assert(page_alloc(&pp) == -E_NO_MEM);

	// forcibly take pp0 back
	assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));
	boot_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	printf("page_check() succeeded!\n");
}



void pageout(int va,int context)
{
	u_long r;
	struct Page *p=NULL;

	if (context<0x80000000)
		panic("tlb refill and alloc error!");
	if ((va>0x7f400000)&&(va<0x7f800000))
		panic(">>>>>>>>>>>>>>>>>>>>>>it's env's zone");

	if (va<0x10000)
		panic("^^^^^^TOO LOW^^^^^^^^^"); 
	if ((r = page_alloc(&p)) < 0)
		panic ("page alloc error!");
	p->pp_ref++;

	page_insert((int*)context, p, VA2PFN(va), PTE_R);
	printf("pageout:\t@@@___0x%x___@@@  ins a page \n",va);
}

