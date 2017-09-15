/**
 * @file rsp.c
<<<<<<< 17f0526fd5e4ba2e8ee809e1b6e4081db9d823f3
 * @brief Hardware Vector Interface
 * @ingroup rsp
 */

#include "libdragon.h"
#include "regsinternal.h"

/** @brief SP DMA busy */
#define SP_STATUS_DMA_BUSY              ( 1 << 2 )
/** @brief SP IO busy */
#define SP_STATUS_IO_BUSY               ( 1 << 4 )
/** @brief SP broke */
#define SP_STATUS_INTERRUPT_ON_BREAK    ( 1 << 6 )
/** @brief SP halted */
#define SP_STATUS_HALTED                ( 1 )

#define SP_STATUS_CLEAR_HALT        0x00001
#define SP_STATUS_SET_HALT          0x00002
#define SP_STATUS_CLEAR_BROKE       0x00004
#define SP_STATUS_CLEAR_INTR        0x00008
#define SP_STATUS_SET_INTR          0x00010
#define SP_STATUS_CLEAR_SSTEP       0x00020
#define SP_STATUS_SET_SSTEP         0x00040
#define SP_STATUS_CLEAR_INTR_BREAK  0x00080
#define SP_STATUS_SET_INTR_BREAK    0x00100
#define SP_STATUS_CLEAR_SIG0        0x00200
#define SP_STATUS_SET_SIG0          0x00400
#define SP_STATUS_CLEAR_SIG1        0x00800
#define SP_STATUS_SET_SIG1          0x01000
#define SP_STATUS_CLEAR_SIG2        0x02000
#define SP_STATUS_SET_SIG2          0x04000
#define SP_STATUS_CLEAR_SIG3        0x08000
#define SP_STATUS_SET_SIG3          0x10000
#define SP_STATUS_CLEAR_SIG4        0x20000
#define SP_STATUS_SET_SIG4          0x40000
#define SP_STATUS_CLEAR_SIG5        0x80000
#define SP_STATUS_SET_SIG5          0x100000
#define SP_STATUS_CLEAR_SIG6        0x200000
#define SP_STATUS_SET_SIG6          0x400000
#define SP_STATUS_CLEAR_SIG7        0x800000
#define SP_STATUS_SET_SIG7          0x1000000

#define SP_DMA_DMEM 0x04000000
#define SP_DMA_IMEM 0x04001000

/** @brief Static structure to address SP registers */
static volatile struct SP_regs_s * const SP_regs = (struct SP_regs_s *)0xa4040000;

/**
 * @brief Wait until the SI is finished with a DMA request
 */
static void __SP_DMA_wait(void)
{
    while (SP_regs->status & (SP_STATUS_DMA_BUSY | SP_STATUS_IO_BUSY)) ;
}

void rsp_init()
{
    /* Make sure RSP is halted */
    *(volatile uint32_t *) 0xA4080000 = SP_DMA_IMEM;
    SP_regs->status = SP_STATUS_SET_HALT;

    return;
}

void load_ucode(void* start, unsigned long size)
{
    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = (void*)SP_DMA_IMEM;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();
    return;
}

void load_data(void* start, unsigned long size)
{
    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = (void*)SP_DMA_DMEM;
    MEMORY_BARRIER();
    SP_regs->rsp_read_length = size - 1;
    MEMORY_BARRIER();

    __SP_DMA_wait();
    enable_interrupts();
    return;
}

void read_ucode(void* start, unsigned long size)
{
    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = (void*)SP_DMA_IMEM;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();
    data_cache_hit_invalidate(start, size);

    enable_interrupts();
    return;
}


void read_data(void* start, unsigned long size)
{
    disable_interrupts();
    __SP_DMA_wait();

    SP_regs->DRAM_addr = start;
    MEMORY_BARRIER();
    SP_regs->RSP_addr = (void*)SP_DMA_DMEM;
    MEMORY_BARRIER();
    SP_regs->rsp_write_length = size - 1;
    MEMORY_BARRIER();
    __SP_DMA_wait();
    data_cache_hit_invalidate(start, size);

    enable_interrupts();
    return;
}


void run_ucode()
{
    // set RSP program counter
    *(volatile uint32_t *) 0xA4080000 = SP_DMA_IMEM;
    SP_regs->status = SP_STATUS_CLEAR_HALT | SP_STATUS_SET_INTR_BREAK;
    return;
}
=======
 * @brief Hardware Coprocessor Interface
 * @ingroup rsp
 */
#include <stdint.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>

#include "libdragon.h"

/**
 * @defgroup rsp Hardware Coprocessor Interface
 * @ingroup libdragon
 * @brief Interface to the hardware coprocessor (RSP).
 *
 * The hardware coprocessor interface sets up and talks with the RSP in order to perform
 * parallel processing of data. The code executed by the RSP is loaded as a library of
 * modules that fit within the 4KB of IMEM. Module functions are queued for execution
 * as the RSP finishes previously queued functions (FIFO queue).
 *
 * Before attempting to queue any functions on the RSP, the hardware coprocessor interface
 * should be initialized with #rsp_init.  After the RSP is no longer needed, be sure
 * to free all resources using #rsp_close.
 *
 * After the interface is initialized, the library should be loaded with #rsp_load_lib.
 * The entry vectors for functions of a particular module can then be fetched using
 * #rsp_lib_fn. The functions can then be used to create a job to perform on the RSP
 * using #rsp_new_job. The job can be queued for execution with #rsp_queue_job, or
 * #rsp_do_job, which also waits until the queued job is finished before returning.
 *
 * @{
 */


job_queue_t jobQueue;


/**
 * @brief Add job to end of queue
 */
static inline void ADD_TAIL( job_queue_t *q, job_t *j )
{
    j->next = 0;
    j->prev = q->tail;
    q->tail = j;
}

/**
 * @brief Remove job from head of queue and return it
 *
 * @return Job from the head of queue.
 */
static inline job_t *REMOVE_HEAD( job_queue_t *q )
{
    job_t *j = q->head;

    if ( j )
    {
        q->head = j->next;
        if ( q->head )
            q->head->prev = NULL;
    }
    else
    {
        q->head = q->tail = NULL;
    }

    return j;
}

/**
 * @brief Remove job from queue
 */
static inline void REMOVE_NODE( job_queue_t *q, job_t *j )
{
    if ( q->head == j)
    {
        q->head = j->next;
        if ( q->head )
            q->head->prev = NULL;
    }
    else if ( q->tail == j)
    {
        q->tail = j->prev;
        if ( q->tail )
            q->tail->next = NULL;
    }
    else
    {
        j->next->prev = j->prev;
        j->prev->next = j->next;
    }
}

/**
 * @brief Lock access to RSP DMA
 *
 * @param[in] wait
 *            Wait until lock successful if set
 *
 * @return Non-zero if RSP already locked. Zero if successfully locked RSP.
 */
uint32_t __rsp_lock( uint32_t wait )
{
    if ( wait )
    {
        while ( ((volatile uint32_t *)0xA4040000)[7] )
            MEMORY_BARRIER();
        return 0;
    }

    return ((volatile uint32_t *)0xA4040000)[7];
}

/**
 * @brief Unlock access to RSP DMA
 */
void __rsp_unlock( )
{
    ((uint32_t *)0xA4040000)[7] = 0;
    MEMORY_BARRIER();
}

/**
 * @brief Wait for RSP DMA
 *
 * @param[in] busy
 *            Wait on DMA not busy (1), or not full (0)
 */
void __rsp_wait_dma( uint32_t busy )
{
    if ( busy )
        while ( ((volatile uint32_t *)0xA4040000)[6] )
            MEMORY_BARRIER(); // wait until not busy
    else
        while ( ((volatile uint32_t *)0xA4040000)[5] )
            MEMORY_BARRIER(); // wait until not full
}

/**
 * @brief Set RSP DMA for read to D/IMEM from DRAM
 *
 * @param[in] offs
 *            Offset into D/IMEM (0x0000 to 0x1FFF - must be 8-byte aligned!)
 * @param[in] src
 *            Address in DRAM of data (must be 8-byte aligned!)
 * @param[in] len
 *            Length in bytes (max of 4KB)
 */
void __rsp_dma_read( uint32_t offs, void *src, uint32_t len )
{
    ((uint32_t *)0xA4040000)[0] = offs & 0x1FFF;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[1] = (uint32_t)src;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[2] = (len - 1) & 0x0FFF;
    MEMORY_BARRIER();
}

/**
 * @brief Set RSP DMA for write to DRAM from D/IMEM
 *
 * @param[in] offs
 *            Offset into D/IMEM (0x0000 to 0x1FFF - must be 8-byte aligned!)
 * @param[in] dst
 *            Address in DRAM for data (must be 8-byte aligned!)
 * @param[in] len
 *            Length in bytes (max of 4KB)
 */
void __rsp_dma_write( uint32_t offs, void *dst, uint32_t len )
{
    ((uint32_t *)0xA4040000)[0] = offs & 0x1FFF;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[1] = (uint32_t)dst;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[3] = (len - 1) & 0x0FFF;
    MEMORY_BARRIER();
}

/**
 * @brief Set RSP DMA for read block to D/IMEM from DRAM
 *
 * @param[in] offs
 *            Offset into D/IMEM (0x0000 to 0x1FFF - must be 8-byte aligned!)
 * @param[in] src
 *            Address in DRAM of data (must be 8-byte aligned!)
 * @param[in] pitch
 *            Number of total columns in pixels (max of 4096/bpp)
 * @param[in] width
 *            Number of columns in pixels (max of 4096/bpp)
 * @param[in] height
 *            Number of rows in lines (max of 256)
 * @param[in] bpp
 *            Number of bytes per pixel (max of 4)
 */
void __rsp_blk_read( uint32_t offs, void *src, uint16_t pitch, uint16_t width, uint16_t height, uint16_t bpp )
{
    ((uint32_t *)0xA4040000)[0] = offs & 0x1FFF;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[1] = (uint32_t)src;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[2] = ((pitch-width)*bpp << 20) | ((height-1) << 12) | (width*bpp - 1);
    MEMORY_BARRIER();
}

/**
 * @brief Set RSP DMA for write block to DRAM from D/IMEM
 *
 * @param[in] offs
 *            Offset into D/IMEM (0x0000 to 0x1FFF - must be 8-byte aligned!)
 * @param[in] dst
 *            Address in DRAM for data (must be 8-byte aligned!)
 * @param[in] pitch
 *            Number of total columns in pixels (max of 4096/bpp)
 * @param[in] width
 *            Number of columns in pixels (max of 4096/bpp)
 * @param[in] height
 *            Number of rows in lines (max of 256)
 * @param[in] bpp
 *            Number of bytes per pixel (max of 4)
 */
void __rsp_blk_write( uint32_t offs, void *dst, uint16_t pitch, uint16_t width, uint16_t height, uint16_t bpp )
{
    ((uint32_t *)0xA4040000)[0] = offs & 0x1FFF;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[1] = (uint32_t)dst;
    MEMORY_BARRIER();
    ((uint32_t *)0xA4040000)[3] = ((pitch-width)*bpp << 20) | ((height-1) << 12) | (width*bpp - 1);
    MEMORY_BARRIER();
}

/**
 * @brief Return the status bits in the RSP
 *
 * @return RSP status bits.
 */
uint32_t __rsp_get_status( )
{
    return ((volatile uint32_t *)0xA4040000)[4];
}

/**
 * @brief Set or clear the status bits in the RSP
 *
 * @param[in] flags
 *            A set of SET and CLR status bits
 */
void __rsp_set_status( uint32_t flags )
{
    ((uint32_t *)0xA4040000)[4] = flags;
    MEMORY_BARRIER();
}

/**
 * @brief Set RSP PC
 *
 * @param[in] pc
 *            Function entry point in IMEM
 */
void __rsp_set_pc( uint32_t pc )
{
    *((uint32_t *)0xA4080000) = pc;
    MEMORY_BARRIER();
}

/**
 * @brief RSP interrupt handler
 *
 * This interrupt is called when a the RSP executes a break instruction. This
 * signifies the job being executed is finished or errored out.
 */
static void __rsp_interrupt()
{
    job_t *j;

    disable_interrupts();

    /* set HALT, clear BROKE, RSP interrupt, and INTERRUPT ON BREAK */
    __rsp_set_status(0x8E);

    j = REMOVE_HEAD(&jobQueue);
    if ( j )
    {
        j->state = JOB_STATE_FINISHED;
        /* do callback of finished job (if any) */
        if ( j->cb )
            j->cb(j);
        j->state = JOB_STATE_IDLE;
    }

    j = jobQueue.head;
    if ( j && j->fn )
    {
        memcpy((void *)(0xA4001000 - 16*4), j->args, 16*4); // copy args to stack
        __rsp_set_pc(j->fn);
        /* clear HALT, SSTEP, and all signals, set INTERRUPT ON BREAK */
        __rsp_set_status(0xAAAB21);
        /* set running flag */
        j->state = JOB_STATE_RUNNING;
    }

    enable_interrupts();
}


/**
 * @brief Initialize the RSP system
 */
void rsp_init( void )
{
    /* Set HALT, clear BROKE, RSP interrupt, and INTERRUPT ON BREAK */
    __rsp_set_status(0x8E);

    /* until until all RSP DMA done */
    __rsp_wait_dma( 1 );

    /* Clear the job queue */
    jobQueue.head = 0;
    jobQueue.tail = 0;

    /* Set up interrupt for RSP BROKE */
    register_SP_handler( __rsp_interrupt );
    set_SP_interrupt( 1 );
}

/**
 * @brief Close the RSP system
 */
void rsp_close( void )
{
    /* Set HALT, clear BROKE, RSP interrupt, and INTERRUPT ON BREAK */
    __rsp_set_status(0x8E);

    /* wait until all RSP DMA done */
    __rsp_wait_dma( 1 );

    /* Clear the job queue */
    jobQueue.head = 0;
    jobQueue.tail = 0;

    set_SP_interrupt( 0 );
    unregister_SP_handler( __rsp_interrupt );
}

/**
 * @brief Load RSP library to D/IMEM
 *
 * There's no need to lock the RSP DMA here since we've halted the RSP.
 * The library must be 8KB long and in DRAM on an 8 byte alignment.
 *
 * @param[in] lib
 *            Pointer to library
 */
void rsp_load_lib( uint8_t *lib )
{
    /* Set HALT, clear BROKE, RSP interrupt, and INTERRUPT ON BREAK */
    __rsp_set_status(0x8E);

    /* wait until all RSP DMA done */
    __rsp_wait_dma( 1 );

    /* Clear the job queue */
    jobQueue.head = 0;
    jobQueue.tail = 0;

    /* transfer data segment to DMEM */
    __rsp_dma_read(0x0000, &lib[0], 0x1000);
    /* wait unti RSP DMA not full */
    __rsp_wait_dma( 0 );
    /* transfer text segment to IMEM */
    __rsp_dma_read(0x1000, &lib[0x1000], 0x1000);

    /* wait until all RSP DMA done */
    __rsp_wait_dma( 1 );
}

/**
 * @brief Return the vector of the function entry point in a module
 *
 * @param[in] mod
 *            Module name string
 * @param[in] fn
 *            Index of function in module
 *
 * @return Vector for function in module inside IMEM.
 */
uint32_t rsp_lib_fn( char *mod, uint32_t fn )
{
    uint32_t *p = (uint32_t *)0xA4001000; // module table at start of IMEM
    uint32_t *f;
    int i = 0;

    while ( p[i] )
    {
        if ( !strcmp((char *)(0xA4000000 + p[i]), mod) )
            break; // found module
        i += 2;
    }
    if ( !p[i] )
        return 0; // couldn't find module!

    f = (uint32_t *)(0xA4000000 + p[i + 1]);
    return f[fn]; // no error checking on function index
}

/**
 * @brief Queue a job for the RSP
 *
 * @param[in] job
 *            Job for execution on the RSP
 */
void rsp_queue_job( job_t *job )
{
    uint32_t s;
    job_t *j;

    disable_interrupts();

    ADD_TAIL(&jobQueue, job);
    job->state = JOB_STATE_QUEUED;

    /* check if RSP running */
    s = __rsp_get_status( );
    if ( s & 3 )
    {
        /* RSP not running, start first job */
        j = jobQueue.head;
        if (j && j->fn)
        {
            memcpy((void *)(0xA4001000 - 16*4), j->args, 16*4); // copy args to stack
            __rsp_set_pc(j->fn);
            /* clear HALT, SSTEP, and all signals, set INTERRUPT ON BREAK */
            __rsp_set_status(0xAAAB21);
            /* set running flag */
            j->state = JOB_STATE_RUNNING;
        }
    }

    enable_interrupts();
}

/**
 * @brief Queue a job for the RSP and wait until it's done
 *
 * @param[in] job
 *            Job for execution on the RSP
 */
void rsp_do_job( job_t *job )
{
    rsp_queue_job( job );
    while ( job->state != JOB_STATE_IDLE )
        MEMORY_BARRIER();
}

/**
 * @brief Wait on a queued job for the RSP
 *
 * @param[in] job
 *            Job to wait on
 */
void rsp_wait_job( job_t *job )
{
    while ( job->state != JOB_STATE_IDLE )
        MEMORY_BARRIER();
}

/**
 * @brief Abort a queued job for the RSP
 *
 * @param[in] job
 *            Job to remove from queue
 *
 * The job may be running. If so, the RSP is forced to stop, the callback is
 * ignored, and the next job is started.
 */
void rsp_abort_job( job_t *job )
{
    uint32_t s;

    disable_interrupts();

    s = __rsp_get_status();
    __rsp_set_status(2); /* set HALT */

    if ( job->state == JOB_STATE_QUEUED )
    {
        REMOVE_NODE(&jobQueue, job);

        /* if RSP was running, clear HALT */
        if ( (s & 3) == 0 )
            __rsp_set_status(1);
    }
    else if ( job->state == JOB_STATE_RUNNING )
    {
        job_t *j;

        /* set HALT, clear BROKE, RSP interrupt, and INTERRUPT ON BREAK */
        __rsp_set_status(0x8E);

        j = REMOVE_HEAD(&jobQueue);

        /* start next job */
        j = jobQueue.head;
        if ( j && j->fn )
        {
            memcpy((void *)(0xA4001000 - 16*4), j->args, 16*4); // copy args to stack
            __rsp_set_pc(j->fn);
            /* clear HALT, SSTEP, and all signals, set INTERRUPT ON BREAK */
            __rsp_set_status(0xAAAB21);
            /* set running flag */
            j->state = JOB_STATE_RUNNING;
        }
    }
    else
    {
        /* if RSP was running, clear HALT */
        if ( (s & 3) == 0 )
            __rsp_set_status(1);
    }

    enable_interrupts();

    job->state = JOB_STATE_IDLE;
}

/**
 * @brief Return a new job structure with fields filled in
 *
 * @param[in] fn
 *            Index of function in module
 * @param[in] cb
 *            callback for the job
 * @param[in] count
 *            number of args passed (max of 16)
 * @param[in] ...
 *            arguments for the job (max of 16)
 *
 * @return New job with fields filled in, or NULL.
 */
job_t *rsp_new_job( uint32_t fn, void (*cb)(job_t *), uint32_t count, ... )
{
    va_list opt;
    job_t *j;
    int i;

    j = (job_t *)malloc(sizeof(job_t));
    if ( j )
    {
        memset(j, 0, sizeof(job_t));
        j->fn = fn;
        j->cb = cb;
        va_start(opt, count);
        for ( i=0; i<count; i++ )
            j->args[i] = va_arg(opt, uint32_t);
        va_end(opt);
    }

    return j;
}

/**
 * @brief Dispose of an existing job structure
 *
 * @param[in] job
 *            Job created by #rsp_new_job
 *
 * Job must not be currently queued or running.
 */
void rsp_dispose_job( job_t *job )
{
    free(job);
}


/** @} */
>>>>>>> Added RSP files
