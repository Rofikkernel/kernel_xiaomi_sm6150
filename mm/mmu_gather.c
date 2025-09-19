#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/mmdebug.h>
#include <linux/mm_types.h>
#include <linux/pagemap.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include <linux/swap.h>

#include <asm/pgalloc.h>
#include <asm/tlb.h>

#ifdef HAVE_GENERIC_MMU_GATHER

static bool tlb_next_batch(struct mmu_gather *tlb)
{
	struct mmu_gather_batch *batch;

	batch = tlb->active;
	if (batch->next) {
		tlb->active = batch->next;
		return true;
	}

	if (tlb->batch_count == MAX_GATHER_BATCH_COUNT)
		return false;

	batch = (void *)__get_free_pages(GFP_NOWAIT | __GFP_NOWARN, 0);
	if (!batch)
		return false;

	tlb->batch_count++;
	batch->next = NULL;
	batch->nr   = 0;
	batch->max  = MAX_GATHER_BATCH;

	tlb->active->next = batch;
	tlb->active = batch;

	return true;
}


void tlb_flush_mmu_free(struct mmu_gather *tlb)
{
	struct mmu_gather_batch *batch;

#ifdef CONFIG_HAVE_RCU_TABLE_FREE
	tlb_table_flush(tlb);
#endif
	for (batch = &tlb->local; batch && batch->nr; batch = batch->next) {
		free_pages_and_swap_cache(batch->pages, batch->nr);
		batch->nr = 0;
	}
	tlb->active = &tlb->local;
}


/* tlb_finish_mmu
 *	Called at the end of the shootdown operation to free up any resources
 *	that were required.
 */


/* __tlb_remove_page
 *	Must perform the equivalent to __free_pte(pte_get_and_clear(ptep)), while
 *	handling the additional races in SMP caused by other CPUs caching valid
 *	mappings in their TLBs. Returns the number of free page slots left.
 *	When out of page slots we must call tlb_flush_mmu().
 *returns true if the caller should flush.
 */


#endif /* HAVE_GENERIC_MMU_GATHER */

#ifdef CONFIG_HAVE_RCU_TABLE_FREE

/*
 * See the comment near struct mmu_table_batch.
 */

/*
 * If we want tlb_remove_table() to imply TLB invalidates.
 */
static inline void tlb_table_invalidate(struct mmu_gather *tlb)
{
#ifdef CONFIG_HAVE_RCU_TABLE_INVALIDATE
	/*
	 * Invalidate page-table caches used by hardware walkers. Then we still
	 * need to RCU-sched wait while freeing the pages because software
	 * walkers can still be in-flight.
	 */
	tlb_flush_mmu_tlbonly(tlb);
#endif
}

static void tlb_remove_table_smp_sync(void *arg)
{
	/* Simply deliver the interrupt */
}

static void tlb_remove_table_one(void *table)
{
	/*
	 * This isn't an RCU grace period and hence the page-tables cannot be
	 * assumed to be actually RCU-freed.
	 *
	 * It is however sufficient for software page-table walkers that rely on
	 * IRQ disabling. See the comment near struct mmu_table_batch.
	 */
	smp_call_function(tlb_remove_table_smp_sync, NULL, 1);
	__tlb_remove_table(table);
}

static void tlb_remove_table_rcu(struct rcu_head *head)
{
	struct mmu_table_batch *batch;
	int i;

	batch = container_of(head, struct mmu_table_batch, rcu);

	for (i = 0; i < batch->nr; i++)
		__tlb_remove_table(batch->tables[i]);

	free_page((unsigned long)batch);
}

#endif /* CONFIG_HAVE_RCU_TABLE_FREE */


