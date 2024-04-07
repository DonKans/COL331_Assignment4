#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"
#include "pageswap.h"
#include "x86.h"

#include "memlayout.h"
#include "elf.h"

struct swap_page swap_space[SWAP_PAGES];

int get_empty_slot(){
    for(int i = 0;i < SWAP_PAGES;i++){
        if(swap_space[i].is_free){
            return i;
        }
    }
    return -1;
};

int get_start_block(int page){
    return 2+ 8*page;
}
void swap_init(){

    for(int i= 0;i < SWAP_PAGES;i++){
        swap_space[i].is_free = 1;
        swap_space[i].page_perm = 0;
    }
}


int get_perm(int i){
    return swap_space[i].page_perm;
}

struct proc* get_victim_proc(){
    struct proc* p = get_proc_list();
   // struct spinlock* lock = get_proc_lock();
    //acquire(lock);
    
    int maximum= 0;
    for(int i =0; i< NPROC ; i++){
            if(((p+i)->state == UNUSED))
                continue;
        maximum = maximum >= (p+i)->rss ? maximum : (p+i)->rss;
    }
    for(int i=0; i< NPROC; i++){
        if(((p+i)->state == UNUSED))
                continue;
    if((p+i)->rss== maximum){
        return (p+i);
    }
    }
    //release(lock);
    return p;
}

static pte_t *
get_page(struct proc* target)
{
  //uint size = target->sz;
  pde_t* directory = target->pgdir;
  int accessed_pages = 0;
  for(int i = 0;i< NPDENTRIES;i++){
    pde_t* pde = &directory[i];
      if(*pde & PTE_P){
        pte_t* pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
        for(int j = 0;j< NPTENTRIES;j++){
            pte_t* curr_page = &pgtab[j];
            if((*curr_page & PTE_P) && !(*curr_page & PTE_A) && !(*curr_page & PTE_S)){
                cprintf("found page \n");
                return curr_page;
            }
            if((*curr_page & PTE_P) && !(*curr_page & PTE_S)&& (*curr_page & PTE_A)){
                accessed_pages += 1;
            }
        }
      }
  }
  int switched = 0.1 * accessed_pages;
  if(switched == 0){switched += 1;}
    for(int i = 0;i< NPDENTRIES;i++){
    pde_t* pde = &directory[i];
      if(*pde & PTE_P){
        pte_t* pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
        for(int j = 0;j< NPTENTRIES;j++){
            pte_t* curr_page = &pgtab[j];
            if(!(*curr_page & PTE_S)&&(*curr_page & PTE_P) && (*curr_page & PTE_A) && switched > 0){
                *curr_page |= PTE_A;
                switched -= 1;
            }
        }
      }
  } 

  for(int i = 0;i< NPDENTRIES;i++){
    pde_t* pde = &directory[i];
      if(*pde & PTE_P){
        pte_t* pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
        for(int j = 0;j< NPTENTRIES;j++){
            pte_t* curr_page = &pgtab[j];
            if(!(*curr_page & PTE_S)&&(*curr_page & PTE_P) && !(*curr_page & PTE_A)){
                return curr_page;
            }
        }
      }
  }
  return 0;
}

void swap_out(){

struct proc* p = get_victim_proc();
cprintf("I got the victim process \n");
pte_t* page = get_page(p);
cprintf("I got the victim page \n");
int my_slot = get_empty_slot();
if(my_slot == -1){
    cprintf("swap_space over");
}
int start_block = get_start_block(my_slot);
swap_space[my_slot].is_free = 0;
swap_space[my_slot].page_perm = PTE_FLAGS(*page);
cprintf("Yahaan tak sab badhiya\n");
struct buf* b;
cprintf("%d \n",start_block);
for(int i = 0;i < 8;i++){


//struct spinlock* lock = get_proc_lock();
//acquire(lock);
 b = bread(ROOTDEV,start_block + i);
//cprintf("bread tak sab badhiya\n");./
memmove((void*)b->data,(void*)(P2V(PTE_ADDR(*page)) + i*BSIZE),BSIZE);
bwrite(b);

brelse(b);
//release(lock);
//cprintf("brelse tak sab badhiya\n");
}
cprintf("%d \n",*page);

uint our_page = *page;
cprintf("%p \n",*page); 
cprintf("%p \n",(char*)(PTE_ADDR(our_page) + KERNBASE));
*page =  (uint)(my_slot<< 12)  + (*page & 0xFFF);
cprintf("%p \n",*page);
cprintf("%p \n",(~PTE_P));

*page = *page & (~PTE_P);
*page = *page | PTE_S;
cprintf("%p \n",*page);
//cprintf("%d \n",*page);


kfree((char*)(P2V_WO(PTE_ADDR(our_page))));
}

void swap_in(){
    uint address  = rcr2();
    pde_t* pgdir = myproc()->pgdir;
  pde_t* pde;
  pte_t *pgtab;
  cprintf("swapping in  \n");
  pde = &pgdir[PDX(address)];
  pgtab = (pte_t*)P2V(PTE_ADDR(*pde));    // Make sure all those PTE_P bits are zero.
  pte_t* page = &pgtab[PTX(address)];
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
  if((*page & PTE_P) == 0){
    int my_slot = (PTE_ADDR(*page)) >> 12;
    int start_block = get_start_block(my_slot);
    char* allocated_page = kalloc();
    struct buf* b;
for(int i = 0;i < 8;i++){

//cprintf("%d \n",start_block + i);
//struct spinlock* lock = get_proc_lock();
//acquire(lock);
 b = bread(ROOTDEV,start_block + i);
//cprintf("bread tak sab badhiya\n");./
memmove((void*)(PTE_ADDR(allocated_page) + i*BSIZE),(void*)b->data,BSIZE);

brelse(b);
//release(lock);
//cprintf("brelse tak sab badhiya\n");
}
*page = 0; 
*page = *page | swap_space[my_slot].page_perm;
*page = *page | V2P(PTE_ADDR(allocated_page));
swap_space[my_slot].is_free = 1;
  }
   
}