#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SIZE_OF_STAT 100
#define BOUND_OF_LOOP 50
//#define SIZE_OF_STAT 100000
//#define BOUND_OF_LOOP 1000
//#define UINT64_MAX (18446744073709551615ULL)

void Filltimes(uint64_t **times)
{
    unsigned long flags = 0;  // 用户空间不需要保存flags
    int i, j;
    uint64_t start, end;
    unsigned cycles_low, cycles_high, cycles_low1, cycles_high1;
    volatile int variable = 0;

    asm volatile ("CPUID\n\t"
		    "RDTSC\n\t"
		    "mov %%edx, %0\n\t"
		    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low)::
		    "%rax", "%rbx", "%rcx", "%rdx");
    asm volatile("RDTSCP\n\t"
		    "mov %%edx, %0\n\t"
		    "mov %%eax, %1\n\t"
		    "CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: "%rax",
		    "%rbx", "%rcx", "%rdx");
    asm volatile ("CPUID\n\t"
		    "RDTSC\n\t"
		    "mov %%edx, %0\n\t"
		    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low)::
		    "%rax", "%rbx", "%rcx", "%rdx");
    asm volatile("RDTSCP\n\t"
		    "mov %%edx, %0\n\t"
		    "mov %%eax, %1\n\t"
		    "CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: "%rax",
		    "%rbx", "%rcx", "%rdx");

    for (j = 0; j < BOUND_OF_LOOP; j++)
    {
        for (i = 0; i < SIZE_OF_STAT; i++)
        {
            variable = 0;

            // 用户空间不需要preempt_disable和irq_save
	    asm volatile (".p2align 6\n\t"
			    "CPUID\n\t"
			    "RDTSC\n\t"
			    "mov %%edx, %0\n\t"
			    "mov %%eax, %1\n\t": "=r" (cycles_high), "=r"
			    (cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");
	    /***********************************/
	    /*call the function to measure here*/
	    /***********************************/
	    asm volatile(".p2align 6\n\t"
			    "RDTSCP\n\t"
			    "mov %%edx, %0\n\t"
			    "mov %%eax, %1\n\t"
			    "CPUID\n\t": "=r" (cycles_high1), "=r"
			    (cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx");

            start = (((uint64_t)cycles_high << 32) | cycles_low);
            end = (((uint64_t)cycles_high1 << 32) | cycles_low1);

            if ((end - start) < 0)
            {
	      printf("\n\n>>>>>>>>>>>>>> CRITICAL ERROR IN TAKING THE TIME!!!!!!\n loop(%d) stat(%d) start = %lu, end = %lu, variable = %u\n", 
		     j, i, (unsigned long)start, (unsigned long)end, variable);
	      times[j][i] = 0;
            }
            else
            {
                times[j][i] = end - start;
            }
        }
    }
    return;
}

static uint64_t var_calc(uint64_t *inputs, int size)
{
    int i;
    uint64_t acc = 0, previous = 0, temp_var = 0;
    
    if (size <= 0) return 0;
    
    for (i = 0; i < size; i++)
    {
        if (acc < previous)
            goto overflow;
        previous = acc;
        acc += inputs[i];
    }
    
    uint64_t acc_squared = acc * acc;
    if (acc_squared < previous)
        goto overflow;
    
    previous = 0;
    for (i = 0; i < size; i++)
    {
        if (temp_var < previous)
            goto overflow;
        previous = temp_var;
        uint64_t square = inputs[i] * inputs[i];
        temp_var += square;
    }
    
    uint64_t size_u64 = (uint64_t)size;
    temp_var = temp_var * size_u64;
    if (temp_var < previous)
        goto overflow;
    
    if (size_u64 * size_u64 == 0)  // 防止除零
        return 0;
        
    temp_var = (temp_var - acc_squared) / (size_u64 * size_u64);
    return temp_var;
    
overflow:
    printf("\n\n>>>>>>>>>>>>>> CRITICAL OVERFLOW ERROR IN var_calc!!!!!!\n\n");
    return 0;  // 返回0而不是-EINVAL
}

int main(void)
{
    int i = 0, j = 0, spurious = 0, k = 0;
    uint64_t **times;
    uint64_t *variances;
    uint64_t *min_values;
    uint64_t max_dev = 0, min_time = 0, max_time = 0, prev_min = 0, tot_var = 0,
             max_dev_all = 0, var_of_vars = 0, var_of_mins = 0;

    printf("Loading benchmark module...\n");

    // 使用calloc分配内存并初始化为0
    times = (uint64_t **)calloc(BOUND_OF_LOOP, sizeof(uint64_t *));
    if (!times)
    {
        printf("unable to allocate memory for times\n");
        return 1;
    }

    for (j = 0; j < BOUND_OF_LOOP; j++)
    {
        times[j] = (uint64_t *)calloc(SIZE_OF_STAT, sizeof(uint64_t));
        if (!times[j])
        {
            printf("unable to allocate memory for times[%d]\n", j);
            for (k = 0; k < j; k++)
                free(times[k]);
            free(times);
            return 1;
        }
    }

    variances = (uint64_t *)calloc(BOUND_OF_LOOP, sizeof(uint64_t));
    if (!variances)
    {
        printf("unable to allocate memory for variances\n");
        for (j = 0; j < BOUND_OF_LOOP; j++)
            free(times[j]);
        free(times);
        return 1;
    }

    min_values = (uint64_t *)calloc(BOUND_OF_LOOP, sizeof(uint64_t));
    if (!min_values)
    {
        printf("unable to allocate memory for min_values\n");
        for (j = 0; j < BOUND_OF_LOOP; j++)
            free(times[j]);
        free(times);
        free(variances);
        return 1;
    }

    Filltimes(times);

    for (j = 0; j < BOUND_OF_LOOP; j++)
    {
        max_dev = 0;
        min_time = 0;
        max_time = 0;

        // 初始化min_time为第一个元素
        if (SIZE_OF_STAT > 0)
        {
            min_time = times[j][0];
            max_time = times[j][0];
        }

        for (i = 0; i < SIZE_OF_STAT; i++)
        {
            if (min_time > times[j][i])
                min_time = times[j][i];
            if (max_time < times[j][i])
                max_time = times[j][i];
        }

        max_dev = max_time - min_time;
        min_values[j] = min_time;

        if ((prev_min != 0) && (prev_min > min_time))
            spurious++;
        if (max_dev > max_dev_all)
            max_dev_all = max_dev;

        variances[j] = var_calc(times[j], SIZE_OF_STAT);
        tot_var += variances[j];

	printf("loop_size:%d \t >>>> variance(cycles): %lu;\tmax_deviation: %lu ;\tmin time: %lu\n", 
               j, (unsigned long)variances[j], (unsigned long)max_dev, (unsigned long)min_time);

	prev_min = min_time;
    }

    var_of_vars = var_calc(variances, BOUND_OF_LOOP);
    var_of_mins = var_calc(min_values, BOUND_OF_LOOP);

    printf("\n total number of spurious min values = %d", spurious);
    printf("\n total variance = %lu", (unsigned long)(tot_var / BOUND_OF_LOOP));
    printf("\n absolute max deviation = %lu", (unsigned long)max_dev_all);
    printf("\n variance of variances = %lu", (unsigned long)var_of_vars);
    printf("\n variance of minimum values = %lu", (unsigned long)var_of_mins);
    printf("\n");

    for (j = 0; j < BOUND_OF_LOOP; j++)
    {
        free(times[j]);
    }
    free(times);
    free(variances);
    free(min_values);
    
    return 0;
}
