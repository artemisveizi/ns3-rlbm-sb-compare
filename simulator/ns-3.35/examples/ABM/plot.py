import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os
from matplotlib.figure import figaspect

w, h = figaspect(1/4.0)

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
        plt.clf()
        line=[]
        fig,(ax0,ax1,ax2)=plt.subplots(1,3,figsize=(w,h))
        for j in range(3):
            ALG=alg[j]
            ALG_name=alg_name[j]
            result_fct=[]
            result_throughput=[]
            result_buffer=[]

            for LOAD in load:       
                flowfile="dump_test/fcts-buffer-{}-{}-{}-{}-9.6.fct".format(CDF,LOAD,TCP,ALG)
                df=pd.read_csv(flowfile,header=0,sep=' ')
                slowdown=df['slowdown'].to_numpy()
                slowdown99=(np.sort(slowdown))[int(0.99*slowdown.size)]
                result_fct.append(slowdown99)

                total_time=np.max(df['time'].to_numpy())-10
                total_size=np.sum(df['flowsize'].to_numpy())
                result_throughput.append(total_size*1.0/total_time)

                bufferfile="dump_test/tor-buffer-{}-{}-{}-{}-9.6.stat".format(CDF,LOAD,TCP,ALG)
                df=pd.read_csv(bufferfile,header=0,sep=' ')
                buffer=df['uplinkThroughput'].to_numpy()
                buffer=np.sort(buffer)
                buffer99=buffer[int(0.99*buffer.size)]
                result_buffer.append(buffer99)

            print(CDF,TCP,ALG,result_fct)
            ax0.plot(load,result_fct,marker='o',label=ALG_name)
            ax1.plot(load,result_throughput,marker='o',label=ALG_name)
            ax2.plot(load,result_buffer,marker='o',label=ALG_name)


        ax0.set(xlabel='load',ylabel='99% FCT Slowdown')
        ax0.legend()
        ax1.set(xlabel='load',ylabel='Average Throughput')
        ax1.legend()
        ax2.set(xlabel='load',ylabel='99% Buffer Occupancy')
        ax2.legend()
        fig.suptitle(CDF+'-'+TCP_name)
        fig.tight_layout()
        # plt.legend()
        plt.savefig("figure_rlbuffer/result_9.6_{}_{}.png".format(CDF,TCP_name))
        plt.savefig("figure_rlbuffer/result_9.6_{}_{}.pdf".format(CDF,TCP_name))

# for CDF in cdf:
#     for i in range(4):
#         TCP=tcp[i]
#         TCP_name=tcp_name[i]
#         plt.clf()
#         line=[]
#         fig,(ax0,ax1,ax2)=plt.subplots(1,3,figsize=(w,h))
#         for j in range(2):
#             ALG=alg[j]
#             ALG_name=alg_name[j]
#             result_fct=[]
#             result_throughput=[]
#             result_buffer=[]

#             for LOAD in load:       
#                 flowfile="dump_test_rlbm/fcts-buffer-{}-{}-{}-{}-0.96.fct".format(CDF,LOAD,TCP,ALG)
#                 df=pd.read_csv(flowfile,header=0,sep=' ',nrows=200000)
#                 slowdown=df['slowdown'].to_numpy()
#                 slowdown99=(np.sort(slowdown))[int(0.99*slowdown.size)]
#                 result_fct.append(slowdown99)

#                 total_time=np.max(df['time'].to_numpy())-10
#                 total_size=np.sum(df['flowsize'].to_numpy())
#                 result_throughput.append(total_size*1.0/total_time)

#                 bufferfile="dump_test_rlbm/tor-buffer-{}-{}-{}-{}-0.96.stat".format(CDF,LOAD,TCP,ALG)
#                 df=pd.read_csv(bufferfile,header=0,sep=' ')
#                 buffer=df['uplinkThroughput'].to_numpy()
#                 buffer=np.sort(buffer)
#                 buffer99=buffer[int(0.99*buffer.size)]
#                 result_buffer.append(buffer99)

#             print(CDF,TCP,ALG,result_fct)
#             ax0.plot(load,result_fct,marker='o',label=ALG_name)
#             ax1.plot(load,result_throughput,marker='o',label=ALG_name)
#             ax2.plot(load,result_buffer,marker='o',label=ALG_name)


#         ax0.set(xlabel='load',ylabel='99% Slowdown')
#         ax0.legend()
#         ax1.set(xlabel='load',ylabel='Average Throughput')
#         ax1.legend()
#         ax2.set(xlabel='load',ylabel='99% Buffer Occupancy')
#         ax2.legend()
#         fig.suptitle(CDF+'-'+TCP_name)
#         fig.tight_layout()
#         # plt.legend()
#         plt.savefig("figure_rlbuffer/result_0.96_{}_{}.png".format(CDF,TCP_name))

# for CDF in cdf:
#     for i in range(4):
#         TCP=tcp[i]
#         TCP_name=tcp_name[i]
#         plt.clf()
#         line=[]
#         for ALG in alg:
#             result_throughput=[]
#             for LOAD in load:       
#                 flowfile="dump_test/fcts-buffer-{}-{}-{}-{}-0.96.fct".format(CDF,LOAD,TCP,ALG)
#                 # if os.path.exists(flowfile):
#                 df=pd.read_csv(flowfile,header=0,sep=' ')
#                 total_time=np.max(df['time'].to_numpy())-10
#                 total_size=np.sum(df['flowsize'].to_numpy())
#                 result_throughput.append(total_size/total_time)
#             # print(CDF,TCP,ALG,result_fct)
#             line_temp,=plt.plot(load,result_throughput,marker='o')
#             line.append(line_temp)
#         plt.xlabel('load')
#         plt.ylabel('average throughput')
#         plt.legend(line,['ABM','RLbuffer'])
#         plt.title(CDF+'-'+TCP_name)
#         plt.savefig("figure_rlbuffer/throughput_{}_{}.png".format(CDF,TCP_name))







# # # print(slowdown99)

# # # df=pd.read_csv("dump_test/fcts-buffer-1-111-0.96.fct",header=0,sep=' ')

# # # slowdown=df['slowdown'].to_numpy()
# # # slowdown99=(np.sort(slowdown))[int(0.99*slowdown.size)]


# # # print(slowdown99)