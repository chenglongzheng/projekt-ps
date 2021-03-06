#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include "system.h"
#include "main.h"
#include "connection.h"

extern struct aids_global_conf aids_conf;

/**
 * Gathers processor load average like the one from uptime shell method.
 *
 * @param avg pointer to initialized structure where the output shall be saved.
 * @returns 0 if everything ok, -1 if getting load failed.
 */
int load_average(struct load_average *avg)
{
	int r, i;
	double load_avg[3];

	r = getloadavg(load_avg, 3);
	if (r == -1)
	{
		return -1;
	} else
	{
		avg -> measures = r;
		for (i = 0; i < r; i++)
			avg -> data[i] = load_avg[i];
		return 0;
	}
}
/**
 * A function for computing the statistical data for the accumulated load data
 *
 * @param load_data An array containing the load data
 * @param avg Pointer to a load_stats structure to contain the data
 */
void generate_load_stats(struct load_average load_data[], struct load_stats *avg) 
{
	double single_var;
	int i;
	
	for (i = 0 ; i < aids_conf.processor_recent ; i += 1) 
	{
		avg -> averages[0] += load_data[0].data[0];
	}
	avg -> averages[0] /= aids_conf.processor_recent;
	
	for (i = 0; i < aids_conf.processor_recent ; i += 1) 
	{
		single_var = ( load_data[0].data[0] - avg -> averages[0] );
		single_var *= single_var;
		avg -> variances[0] += single_var;
	}
	avg -> variances[0] /= aids_conf.processor_recent;

	avg -> deviations[0] = sqrt(avg -> variances[0]);
}


/**
 * A method for checking whether the averages are raising
 *
 * @param averages An array of statistic averages
 */
int system_averages_rising(double averages[])
{
	int i, result = 1;

	for(i = 0; i < aids_conf.processor_recent-1 ; i++) 
	{
		if(averages[i+1] < averages[i]) 
		{
			result = 0;
		}
	}

	return result;
}

/**
 * Infinite loop gathering network data. This method shall be ran threaded.
 * Writes gathered data to file, analyses it and sends appropriate warnings
 * if something is suspicious.
 * Filename and sleep time are taken from configuration file.
 */
void aids_gather_processor_load(void)
{
	struct load_average load;
	struct load_average recent_load[aids_conf.processor_recent];
	struct load_stats avg;
	double averages[aids_conf.processor_recent];
    int i, j,averages_count = 0;
	FILE* f;

	while (1)
	{
		f = fopen(aids_conf.processor_global_data_filename, "a");
		if (f == NULL)
		{
			perror("[system.c] Couldn't open file!");
			pthread_exit(NULL);
		}
		for(i = 0; i < aids_conf.processor_recent; i += 1)
		{
			load_average(&load);
			logger(stdout, DEBUG, "%d\n", load.measures);
			for(j = 0; j < load.measures-1 ; j+=1)
			{
				logger(stdout, DEBUG, "%lf,", load.data[j]);
			}
			logger(stdout, DEBUG, "%lf\n", load.data[load.measures-1]);
			memcpy(&recent_load[i], &load, sizeof(struct load_average));
			sleep(aids_conf.processor_sleep_time);
		}
		generate_load_stats(recent_load, &avg);
		averages[averages_count] = avg.averages[0];
		averages_count++;
		logger(stdout, DEBUG,  "[system.c] a: %.3g, v: %.3g, stdev: %.3g", avg.averages[0], avg.variances[0], avg.deviations[0]);
		logger(f, DEBUG, "a: %.3g, v: %.3g, stdev: %.3g", avg.averages[0], avg.variances[0], avg.deviations[0]);
		if((0.7)*avg.averages[0] < avg.deviations[0]) 
		{
			send_message("[system.c] Warning! System load weird!");
			logger(stdout, WARN, "[system.c] Warning, deviance is large");
		}
		if (averages_count == aids_conf.processor_recent) 
		{
			if(system_averages_rising(averages))
			{
				send_message("[system.c] Warning! Processor load averages are raising constantly");
				logger(stdout, WARN, "[system.c] warning, averages are raising");
			}
			averages_count = 0;
		}

		fclose(f);
		
	}
}
