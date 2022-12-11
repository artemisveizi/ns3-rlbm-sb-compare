import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from matplotlib.figure import figaspect

w, h = figaspect(2.0)

alg=['101',"110","111"]
alg_name=['DT','ABM','RLBM']
tcp=["1","2","6","4"]
tcp_name=['CUBIC', 'DCTCP', 'TIMELY', 'POWERTCP']
cdf=["websearch", "datamining", "hadoop"]
load=[0.2,0.4,0.6,0.8]

plt.axes(yscale='log')

for CDF in cdf:
    for i in range(1):
        TCP=tcp[i]
        TCP_name=tcp_name[i]        
        for LOAD in load:
            plt.clf()
            line=[]
            fig,axes=plt.subplots(8,1,figsize=(15,20))
            for priority in range(8):
                ax=axes[priority]
                for j in range(3):
                    ALG=alg[j]
                    ALG_name=alg_name[j]
                    
                    queuefile="dump_test/fcts-buffer-{}-{}-{}-{}-9.6.fct.log".format(CDF,LOAD,TCP,ALG)
                    df=pd.read_csv(queuefile,names=range(8),sep=' ',index_col=False)
                    rows=np.array(range(df.shape[0]))*10
                    # print(df.head())
                    # exit()        
                    ax.plot(rows,df[priority],label=ALG_name,linewidth=0.5)
                ax.legend()
                ax.set(xlabel='number of packet (as a measurement of time)',ylabel='queue length of priority {}'.format(priority))
            fig.suptitle(CDF+'-'+TCP_name+'-Load-{}'.format(LOAD),x=0.5,y=1,fontsize=16,fontweight='bold')
            fig.tight_layout()
            plt.savefig("figure_rlbuffer/queue_9.6_{}_{}_Load-{}.pdf".format(CDF,TCP_name,LOAD),dpi=200)
            plt.savefig("figure_rlbuffer/queue_9.6_{}_{}_Load-{}.png".format(CDF,TCP_name,LOAD),dpi=200)
            
            #     slowdown=df['slowdown'].to_numpy()
            #     slowdown99=(np.sort(slowdown))[int(0.99*slowdown.size)]
            #     result_fct.append(slowdown99)

            #     total_time=np.max(df['time'].to_numpy())-10
            #     total_size=np.sum(df['flowsize'].to_numpy())
            #     result_throughput.append(total_size*1.0/total_time)

            #     bufferfile="dump_test/tor-buffer-{}-{}-{}-{}-9.6.stat".format(CDF,LOAD,TCP,ALG)
            #     df=pd.read_csv(bufferfile,header=0,sep=' ')
            #     buffer=df['uplinkThroughput'].to_numpy()
            #     buffer=np.sort(buffer)
            #     buffer99=buffer[int(0.99*buffer.size)]
            #     result_buffer.append(buffer99)

            # print(CDF,TCP,ALG,result_fct)
            # ax0.plot(load,result_fct,marker='o',label=ALG_name)
            # ax1.plot(load,result_throughput,marker='o',label=ALG_name)
            # ax2.plot(load,result_buffer,marker='o',label=ALG_name)


        # ax0.set(xlabel='load',ylabel='99% Slowdown')
        # ax0.legend()
        # ax1.set(xlabel='load',ylabel='Average Throughput')
        # ax1.legend()
        # ax2.set(xlabel='load',ylabel='99% Buffer Occupancy')
        # ax2.legend()
        # fig.suptitle(CDF+'-'+TCP_name)
        # fig.tight_layout()
        # # plt.legend()
        # plt.savefig("figure_rlbuffer/result_9.6_{}_{}.png".format(CDF,TCP_name))
        # plt.savefig("figure_rlbuffer/result_9.6_{}_{}.pdf".format(CDF,TCP_name))